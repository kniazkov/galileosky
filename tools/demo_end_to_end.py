#!/usr/bin/env python3
"""Показывает сквозной обмен с desktop-приложением через JSONL-протокол."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


class DemoFailure(RuntimeError):
    """Описывает ошибку запуска или неожиданный ответ приложения."""


def configure_console_encoding() -> None:
    """Включает UTF-8 для одинакового вывода в Linux и Windows."""

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")


def default_executable() -> Path:
    """Возвращает стандартный путь к desktop-приложению для текущей ОС."""

    project_dir = Path(__file__).resolve().parent.parent
    if sys.platform == "win32":
        return (
            project_dir
            / "build"
            / "windows-desktop"
            / "galileosky_test_project.exe"
        )
    return (
        project_dir
        / "build"
        / "linux-desktop"
        / "galileosky_test_project"
    )


def parse_arguments() -> argparse.Namespace:
    """Разбирает необязательный путь к проверяемому приложению."""

    parser = argparse.ArgumentParser(
        description="Сквозная демонстрация CAN → BASIC → сервер"
    )
    parser.add_argument(
        "--executable",
        type=Path,
        default=default_executable(),
        help="путь к desktop-приложению",
    )
    return parser.parse_args()


def exchange(
    process: subprocess.Popen[str],
    request: dict[str, Any],
) -> list[dict[str, Any]]:
    """Отправляет команду, печатает обмен и читает ответ до сообщения done."""

    if process.stdin is None or process.stdout is None:
        raise DemoFailure("не удалось открыть stdin/stdout приложения")

    request_text = json.dumps(
        request,
        ensure_ascii=False,
        separators=(",", ":"),
    )
    print(f"→ {request_text}")
    process.stdin.write(request_text + "\n")
    process.stdin.flush()

    response: list[dict[str, Any]] = []
    while True:
        line = process.stdout.readline()
        if line == "":
            raise DemoFailure("приложение завершилось до полного ответа")

        print(f"← {line.rstrip()}")
        try:
            message = json.loads(line)
        except json.JSONDecodeError as error:
            raise DemoFailure("приложение вернуло некорректный JSON") from error

        if not isinstance(message, dict) or message.get("id") != request["id"]:
            raise DemoFailure("получен ответ с неожиданным идентификатором")
        response.append(message)
        if message.get("type") == "error":
            raise DemoFailure(f"приложение вернуло ошибку: {message}")
        if message.get("type") == "done":
            return response


def has_server_message(
    response: list[dict[str, Any]],
    expected_speed: int,
) -> bool:
    """Проверяет серверное сообщение BASIC с заданной скоростью."""

    expected_payload = [
        0x20,
        expected_speed & 0xFF,
        (expected_speed >> 8) & 0xFF,
        (expected_speed >> 16) & 0xFF,
        (expected_speed >> 24) & 0xFF,
    ]
    return any(
        message.get("type") == "server_tx"
        and message.get("payload") == expected_payload
        for message in response
    )


def run_demo(executable: Path) -> None:
    """Запускает приложение и проверяет весь путь события превышения скорости."""

    if not executable.is_file():
        raise DemoFailure(
            f"не найдено приложение {executable}; сначала собери desktop-версию"
        )

    print("Сценарий: CAN → OBD-II → автомобиль → BASIC → сервер")
    print(f"Приложение: {executable}")
    process = subprocess.Popen(
        [str(executable)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )

    try:
        exchange(process, {"id": 1, "command": "tick", "timestamp_ms": 0})
        exchange(process, {"id": 2, "command": "server_online", "online": True})

        normal_speed = exchange(
            process,
            {
                "id": 3,
                "command": "can_rx",
                "can_id": 2024,
                "data": [3, 65, 13, 120, 0, 0, 0, 0],
            },
        )
        if any(message.get("type") == "server_tx" for message in normal_speed):
            raise DemoFailure("при скорости 120 км/ч сообщение отправляться не должно")

        overspeed = exchange(
            process,
            {
                "id": 4,
                "command": "can_rx",
                "can_id": 2024,
                "data": [3, 65, 13, 145, 0, 0, 0, 0],
            },
        )
        if not has_server_message(overspeed, 145):
            raise DemoFailure("не получено серверное сообщение о скорости 145 км/ч")

        exchange(process, {"id": 5, "command": "snapshot"})
        exchange(process, {"id": 6, "command": "shutdown"})
        return_code = process.wait(timeout=2.0)
        if return_code != 0:
            raise DemoFailure(f"приложение завершилось с кодом {return_code}")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    print("Сквозной сценарий завершён успешно.")


def main() -> int:
    """Запускает демонстрацию и возвращает подходящий код завершения."""

    configure_console_encoding()
    arguments = parse_arguments()
    try:
        run_demo(arguments.executable.resolve())
    except (DemoFailure, OSError, subprocess.SubprocessError) as error:
        print(f"Ошибка демонстрации: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
