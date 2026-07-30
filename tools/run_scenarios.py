#!/usr/bin/env python3
"""Запускает JSON-сценарии против desktop-версии приложения."""

from __future__ import annotations

import argparse
import json
import queue
import subprocess
import sys
import threading
from pathlib import Path
from typing import Any, TextIO


class ScenarioFailure(RuntimeError):
    """Описывает несовпадение протокола с эталонным сценарием."""


def read_stdout(stream: TextIO, lines: queue.Queue[str | None]) -> None:
    """Передаёт строки stdout в очередь, чтобы основной поток мог ждать с тайм-аутом."""

    for line in stream:
        lines.put(line)
    lines.put(None)


def read_stderr(stream: TextIO, output: list[str]) -> None:
    """Сохраняет диагностический поток процесса для сообщения об ошибке."""

    output.extend(stream)


def receive_response(
    lines: queue.Queue[str | None],
    request_id: int,
    timeout: float,
) -> list[dict[str, Any]]:
    """Читает ответ одной команды до завершающего сообщения done или error."""

    response: list[dict[str, Any]] = []
    while True:
        try:
            line = lines.get(timeout=timeout)
        except queue.Empty as error:
            raise ScenarioFailure(
                f"истёк тайм-аут ответа на команду {request_id}"
            ) from error

        if line is None:
            raise ScenarioFailure(
                f"процесс завершил stdout до ответа на команду {request_id}"
            )

        try:
            message = json.loads(line)
        except json.JSONDecodeError as error:
            raise ScenarioFailure(f"получена некорректная строка JSON: {line!r}") from error

        if not isinstance(message, dict):
            raise ScenarioFailure(f"ожидался объект JSON, получено: {message!r}")
        if message.get("id") != request_id:
            raise ScenarioFailure(
                f"ожидался ответ на команду {request_id}, получено: {message!r}"
            )

        response.append(message)
        if message.get("type") in {"done", "error"}:
            return response


def run_scenario(executable: Path, scenario_path: Path, timeout: float) -> None:
    """Запускает один сценарий в отдельном процессе и сверяет все ответы."""

    with scenario_path.open(encoding="utf-8") as scenario_file:
        scenario = json.load(scenario_file)

    if not isinstance(scenario, dict) or not isinstance(scenario.get("steps"), list):
        raise ScenarioFailure(f"{scenario_path}: отсутствует массив steps")

    process = subprocess.Popen(
        [str(executable)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )
    if process.stdin is None or process.stdout is None or process.stderr is None:
        process.kill()
        raise ScenarioFailure("не удалось открыть каналы дочернего процесса")

    stdout_lines: queue.Queue[str | None] = queue.Queue()
    stderr_lines: list[str] = []
    stdout_thread = threading.Thread(
        target=read_stdout,
        args=(process.stdout, stdout_lines),
        daemon=True,
    )
    stderr_thread = threading.Thread(
        target=read_stderr,
        args=(process.stderr, stderr_lines),
        daemon=True,
    )
    stdout_thread.start()
    stderr_thread.start()

    try:
        for number, step in enumerate(scenario["steps"], start=1):
            if not isinstance(step, dict):
                raise ScenarioFailure(f"шаг {number}: ожидался объект")

            request = step.get("send")
            expected = step.get("expect")
            if not isinstance(request, dict) or not isinstance(expected, list):
                raise ScenarioFailure(
                    f"шаг {number}: обязательны объект send и массив expect"
                )
            request_id = request.get("id")
            if not isinstance(request_id, int) or isinstance(request_id, bool):
                raise ScenarioFailure(f"шаг {number}: id должен быть целым числом")

            process.stdin.write(
                json.dumps(request, ensure_ascii=False, separators=(",", ":")) + "\n"
            )
            process.stdin.flush()

            actual = receive_response(stdout_lines, request_id, timeout)
            if actual != expected:
                expected_text = json.dumps(expected, ensure_ascii=False, indent=2)
                actual_text = json.dumps(actual, ensure_ascii=False, indent=2)
                raise ScenarioFailure(
                    f"шаг {number}: ответ не совпал с эталоном\n"
                    f"ожидалось:\n{expected_text}\n"
                    f"получено:\n{actual_text}"
                )

        process.stdin.close()
        try:
            return_code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise ScenarioFailure("процесс не завершился после сценария") from error
        if return_code != 0:
            raise ScenarioFailure(f"процесс завершился с кодом {return_code}")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        stdout_thread.join(timeout=timeout)
        stderr_thread.join(timeout=timeout)

        if sys.exc_info()[0] is not None and stderr_lines:
            sys.stderr.write("stderr приложения:\n")
            sys.stderr.writelines(stderr_lines)


def parse_arguments() -> argparse.Namespace:
    """Разбирает путь к приложению, сценариям и тайм-аут."""

    parser = argparse.ArgumentParser(
        description="Проверка desktop-приложения по эталонным JSON-сценариям"
    )
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("scenarios", nargs="+", type=Path)
    return parser.parse_args()


def main() -> int:
    """Запускает все переданные сценарии и возвращает код результата."""

    arguments = parse_arguments()
    try:
        for scenario in arguments.scenarios:
            run_scenario(arguments.executable, scenario, arguments.timeout)
            print(f"Сценарий пройден: {scenario}")
    except (OSError, ValueError, ScenarioFailure) as error:
        print(f"Ошибка сценария: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
