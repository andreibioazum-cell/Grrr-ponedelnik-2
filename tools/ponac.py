#!/usr/bin/env python3
"""Pona AOT compiler: Lua-like typed source to ordinary C, never bytecode."""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class PonaError(Exception):
    def __init__(self, message: str, line: int, column: int):
        super().__init__(f"{line}:{column}: {message}")
        self.line, self.column = line, column


@dataclass(frozen=True)
class Token:
    kind: str
    value: Any
    line: int
    column: int


@dataclass
class Expr:
    kind: str
    value: Any = None
    left: Expr | None = None
    right: Expr | None = None
    args: list[Expr] | None = None
    line: int = 1
    type: str = "?"


@dataclass
class Stmt:
    kind: str
    name: str = ""
    annotation: str | None = None
    op: str = ""
    expr: Expr | None = None
    body: list[Stmt] | None = None
    otherwise: list[Stmt] | None = None
    start: Expr | None = None
    stop: Expr | None = None
    step: Expr | None = None
    line: int = 1


@dataclass
class Global:
    name: str
    annotation: str | None
    expr: Expr
    attributes: tuple[str, ...]
    line: int
    type: str = "?"


@dataclass
class Function:
    name: str
    params: list[tuple[str, str]]
    result: str
    body: list[Stmt]
    attributes: tuple[str, ...]
    line: int


@dataclass
class Program:
    globals: list[Global]
    functions: list[Function]


TYPES = {"f32", "i32", "bool", "color", "str", "array", "void"}
KEYWORDS = {"global", "function", "local", "if", "then", "else", "end", "while", "do", "for",
            "return", "break", "true", "false", "nil", "and", "or", "not"}
BUILTINS: dict[str, tuple[list[str], str, str]] = {
    "clear": (["color"], "void", "p2d_clear"),
    "rect": (["f32", "f32", "f32", "f32", "color"], "void", "p2d_rect"),
    "roundrect": (["f32", "f32", "f32", "f32", "f32", "color"], "void", "p2d_roundrect"),
    "circle": (["f32", "f32", "f32", "color"], "void", "p2d_circle"),
    "line": (["f32", "f32", "f32", "f32", "f32", "color"], "void", "p2d_line"),
    "sprite": (["str", "f32", "f32", "f32"], "void", "p2d_sprite"),
    "text": (["str", "f32", "f32", "color"], "void", "p2d_text"),
    "text_scaled": (["str", "f32", "f32", "color", "f32"], "void", "p2d_text"),
    "key_down": (["str"], "bool", "p2d_key_down"),
    "key_pressed": (["str"], "bool", "p2d_key_pressed"),
    "key_released": (["str"], "bool", "p2d_key_released"),
    "pointer_x": ([], "f32", "p2d_pointer_x_value"),
    "pointer_y": ([], "f32", "p2d_pointer_y_value"),
    "pointer_down": ([], "bool", "p2d_pointer_is_down"),
    "pointer_pressed": ([], "bool", "p2d_pointer_is_pressed"),
    "pointer_released": ([], "bool", "p2d_pointer_is_released"),
    "time": ([], "f32", "p2d_time"),
    "random": ([], "f32", "p2d_random"),
    "screen_width": ([], "f32", "p2d_screen_width"),
    "screen_height": ([], "f32", "p2d_screen_height"),
    "sin": (["f32"], "f32", "sinf"),
    "cos": (["f32"], "f32", "cosf"),
    "sqrt": (["f32"], "f32", "sqrtf"),
    "abs": (["f32"], "f32", "fabsf"),
    "min": (["f32", "f32"], "f32", "fminf"),
    "max": (["f32", "f32"], "f32", "fmaxf"),
    "clamp": (["f32", "f32", "f32"], "f32", "p2d_clamp"),
    "lerp": (["f32", "f32", "f32"], "f32", "p2d_lerp"),
    "array_new": ([], "array", "pona_array_new"),
    "array_push_num": (["array", "f32"], "void", "pona_array_push_num"),
    "array_push_str": (["array", "str"], "void", "pona_array_push_str"),
    "array_get_num": (["array", "i32"], "f32", "pona_array_get_num"),
    "array_get_str": (["array", "i32"], "str", "pona_array_get_obj"),
    "array_len": (["array"], "i32", "p2d_array_length"),
    "array_clear": (["array"], "void", "pona_array_clear"),
    "gc_bytes": ([], "i32", "p2d_gc_bytes"),
    "gc_objects": ([], "i32", "p2d_gc_objects"),
    "to_string": (["f32"], "str", "pona_number_string"),
}


def lex(source: str) -> list[Token]:
    tokens: list[Token] = []
    i, line, column = 0, 1, 1
    two_char = {"==", "~=", "!=", "<=", ">=", "+=", "-=", "*=", "/=", ".."}
    singles = set("+-*/%<>=(),:@")
    while i < len(source):
        ch = source[i]
        if ch in " \t\r":
            i += 1
            column += 1
            continue
        if ch == "\n" or ch == ";":
            tokens.append(Token("NEWLINE", "\n", line, column))
            if ch == "\n":
                line += 1
                column = 1
            else:
                column += 1
            i += 1
            continue
        if source.startswith("--", i):
            while i < len(source) and source[i] != "\n":
                i += 1
                column += 1
            continue
        if ch == '"':
            start, start_col = i, column
            i += 1
            column += 1
            escaped = False
            while i < len(source):
                current = source[i]
                if current == "\n" and not escaped:
                    raise PonaError("строка не закрыта до конца строки", line, start_col)
                if current == '"' and not escaped:
                    i += 1
                    column += 1
                    break
                if current == "\\" and not escaped:
                    escaped = True
                else:
                    escaped = False
                i += 1
                column += 1
            else:
                raise PonaError("строка не закрыта", line, start_col)
            try:
                value = json.loads(source[start:i])
            except json.JSONDecodeError as error:
                raise PonaError("неверная escape-последовательность", line, start_col) from error
            if any(0xD800 <= ord(char) <= 0xDFFF for char in value):
                raise PonaError("строка содержит непарный Unicode-суррогат", line, start_col)
            tokens.append(Token("STRING", value, line, start_col))
            continue
        if ch == "#":
            start_col = column
            match = re.match(r"#[0-9A-Fa-f]{6}(?:[0-9A-Fa-f]{2})?", source[i:])
            if not match:
                raise PonaError("цвет должен иметь вид #RRGGBB или #AARRGGBB", line, column)
            raw = match.group()[1:]
            value = int(raw, 16) if len(raw) == 8 else 0xFF000000 | int(raw, 16)
            tokens.append(Token("COLOR", value, line, start_col))
            i += len(match.group())
            column += len(match.group())
            continue
        if ch.isdigit() or (ch == "." and i + 1 < len(source) and source[i + 1].isdigit()):
            start_col = column
            match = re.match(r"(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?", source[i:])
            assert match
            raw = match.group()
            tokens.append(Token("NUMBER", raw, line, start_col))
            i += len(raw)
            column += len(raw)
            continue
        if ch.isalpha() or ch == "_":
            start_col = column
            match = re.match(r"[A-Za-z_][A-Za-z_0-9]*", source[i:])
            assert match
            word = match.group()
            tokens.append(Token("IDENT", word, line, start_col))
            i += len(word)
            column += len(word)
            continue
        pair = source[i:i + 2]
        if pair in two_char:
            tokens.append(Token("OP", pair, line, column))
            i += 2
            column += 2
            continue
        if ch in singles:
            kind = "OP" if ch in "+-*/%<>=" else "PUNCT"
            tokens.append(Token(kind, ch, line, column))
            i += 1
            column += 1
            continue
        raise PonaError(f"неожиданный символ {ch!r}", line, column)
    tokens.append(Token("EOF", "", line, column))
    return tokens


class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0

    @property
    def token(self) -> Token:
        return self.tokens[self.pos]

    def advance(self) -> Token:
        token = self.token
        self.pos += 1
        return token

    def match(self, value: str) -> bool:
        if self.token.value == value:
            self.advance()
            return True
        return False

    def expect(self, value: str) -> Token:
        if self.token.value != value:
            raise PonaError(f"ожидалось {value!r}, получено {self.token.value!r}", self.token.line, self.token.column)
        return self.advance()

    def identifier(self) -> Token:
        if self.token.kind != "IDENT" or self.token.value in KEYWORDS:
            raise PonaError("ожидалось допустимое имя", self.token.line, self.token.column)
        return self.advance()

    def separators(self) -> None:
        while self.token.kind == "NEWLINE":
            self.advance()

    def parse_type(self) -> str:
        token = self.identifier()
        if token.value not in TYPES:
            raise PonaError(f"неизвестный тип {token.value!r}", token.line, token.column)
        return token.value

    def parse(self) -> Program:
        globals_: list[Global] = []
        functions: list[Function] = []
        self.separators()
        while self.token.kind != "EOF":
            attributes: list[str] = []
            while self.match("@"):
                attr = self.identifier()
                if attr.value not in {"export"}:
                    raise PonaError(f"неизвестный атрибут @{attr.value}", attr.line, attr.column)
                attributes.append(attr.value)
                self.separators()
            if self.match("global"):
                globals_.append(self.global_decl(tuple(attributes)))
            elif self.match("function"):
                functions.append(self.function_decl(tuple(attributes)))
            else:
                raise PonaError("на верхнем уровне разрешены global и function", self.token.line, self.token.column)
            self.separators()
        names = [g.name for g in globals_]
        if len(names) != len(set(names)):
            raise PonaError("имя global объявлено повторно", 1, 1)
        function_names = [f.name for f in functions]
        if len(function_names) != len(set(function_names)):
            raise PonaError("имя function объявлено повторно", 1, 1)
        return Program(globals_, functions)

    def global_decl(self, attributes: tuple[str, ...]) -> Global:
        name = self.identifier()
        annotation = self.parse_type() if self.match(":") else None
        self.expect("=")
        expr = self.expression()
        return Global(name.value, annotation, expr, attributes, name.line)

    def function_decl(self, attributes: tuple[str, ...]) -> Function:
        if attributes:
            raise PonaError("@export применяется только к global", self.token.line, self.token.column)
        name = self.identifier()
        self.expect("(")
        params: list[tuple[str, str]] = []
        if self.token.value != ")":
            while True:
                param = self.identifier()
                self.expect(":")
                params.append((param.value, self.parse_type()))
                if not self.match(","):
                    break
        self.expect(")")
        result = self.parse_type() if self.match(":") else "void"
        if result == "array" or result == "str" or result == "color":
            pass
        self.separators()
        body = self.block({"end"})
        self.expect("end")
        param_names = [p[0] for p in params]
        if len(param_names) != len(set(param_names)):
            raise PonaError("имя аргумента повторяется", name.line, name.column)
        return Function(name.value, params, result, body, attributes, name.line)

    def block(self, stops: set[str]) -> list[Stmt]:
        statements: list[Stmt] = []
        self.separators()
        while self.token.kind != "EOF" and self.token.value not in stops:
            statements.append(self.statement())
            if self.token.kind == "NEWLINE":
                self.separators()
            elif self.token.value not in stops:
                raise PonaError("ожидался конец строки", self.token.line, self.token.column)
        return statements

    def statement(self) -> Stmt:
        token = self.token
        if self.match("local"):
            name = self.identifier()
            annotation = self.parse_type() if self.match(":") else None
            self.expect("=")
            return Stmt("local", name.value, annotation, expr=self.expression(), line=token.line)
        if self.match("if"):
            condition = self.expression()
            self.expect("then")
            self.separators()
            body = self.block({"else", "end"})
            otherwise: list[Stmt] = []
            if self.match("else"):
                self.separators()
                otherwise = self.block({"end"})
            self.expect("end")
            return Stmt("if", expr=condition, body=body, otherwise=otherwise, line=token.line)
        if self.match("while"):
            condition = self.expression()
            self.expect("do")
            self.separators()
            body = self.block({"end"})
            self.expect("end")
            return Stmt("while", expr=condition, body=body, line=token.line)
        if self.match("for"):
            name = self.identifier()
            self.expect("=")
            start = self.expression()
            self.expect(",")
            stop = self.expression()
            step = self.expression() if self.match(",") else Expr("number", "1", line=token.line, type="i32")
            self.expect("do")
            self.separators()
            body = self.block({"end"})
            self.expect("end")
            return Stmt("for", name.value, body=body, start=start, stop=stop, step=step, line=token.line)
        if self.match("return"):
            expr = None if self.token.kind in {"NEWLINE", "EOF"} or self.token.value in {"end", "else"} else self.expression()
            return Stmt("return", expr=expr, line=token.line)
        if self.match("break"):
            return Stmt("break", line=token.line)
        expr = self.expression()
        if self.token.value in {"=", "+=", "-=", "*=", "/="}:
            if expr.kind != "name":
                raise PonaError("присваивать можно только переменной", token.line, token.column)
            op = self.advance().value
            return Stmt("assign", name=expr.value, op=op, expr=self.expression(), line=token.line)
        return Stmt("expr", expr=expr, line=token.line)

    PRECEDENCE = {"or": 1, "and": 2, "==": 3, "~=": 3, "!=": 3, "<": 3, ">": 3,
                  "<=": 3, ">=": 3, "..": 4, "+": 5, "-": 5, "*": 6, "/": 6, "%": 6}

    def expression(self, minimum: int = 1) -> Expr:
        token = self.advance()
        if token.value == "not":
            left = Expr("unary", token.value, left=self.expression(3), line=token.line)
        elif token.value == "-":
            left = Expr("unary", token.value, left=self.expression(7), line=token.line)
        elif token.value == "(":
            left = self.expression()
            self.expect(")")
        elif token.kind == "NUMBER":
            left = Expr("number", token.value, line=token.line)
        elif token.kind == "COLOR":
            left = Expr("color", token.value, line=token.line)
        elif token.kind == "STRING":
            left = Expr("string", token.value, line=token.line)
        elif token.value in {"true", "false"}:
            left = Expr("bool", token.value == "true", line=token.line)
        elif token.kind == "IDENT":
            if token.value == "nil":
                left = Expr("nil", line=token.line)
            elif self.match("("):
                args: list[Expr] = []
                if self.token.value != ")":
                    while True:
                        args.append(self.expression())
                        if not self.match(","):
                            break
                self.expect(")")
                left = Expr("call", token.value, args=args, line=token.line)
            else:
                left = Expr("name", token.value, line=token.line)
        else:
            raise PonaError("ожидалось выражение", token.line, token.column)
        while True:
            op = str(self.token.value)
            precedence = self.PRECEDENCE.get(op, 0)
            if precedence < minimum:
                break
            operator = self.advance()
            right = self.expression(precedence + (0 if op == ".." else 1))
            left = Expr("binary", op, left=left, right=right, line=operator.line)
        return left


def c_string(value: str) -> str:
    """Encode UTF-8 bytes with unambiguous C escapes."""
    escaped = ['"']
    for byte in value.encode("utf-8"):
        if byte == 34:
            escaped.append('\\"')
        elif byte == 92:
            escaped.append("\\\\")
        elif byte == 10:
            escaped.append("\\n")
        elif byte == 13:
            escaped.append("\\r")
        elif byte == 9:
            escaped.append("\\t")
        elif 32 <= byte < 127:
            escaped.append(chr(byte))
        else:
            escaped.append(f"\\{byte:03o}")
    escaped.append('"')
    return "".join(escaped)


def c_type(type_name: str) -> str:
    return {"f32": "float", "i32": "int32_t", "bool": "int", "color": "uint32_t",
            "str": "PonaObject *", "array": "PonaObject *", "void": "void"}[type_name]


def compatible(expected: str, actual: str) -> bool:
    return (expected == actual or (expected == "f32" and actual == "i32")
            or (expected in {"str", "array"} and actual == "nil"))


class Compiler:
    def __init__(self, program: Program, source_name: str):
        self.program = program
        self.source_name = source_name
        self.global_types: dict[str, str] = {}
        self.global_attrs: dict[str, tuple[str, ...]] = {}
        self.function_types = {f.name: ([t for _, t in f.params], f.result) for f in program.functions}
        self.function_c_names = {f.name: "pona_user_" + f.name for f in program.functions}
        self.lines: list[str] = []
        self.indent = 0
        self.scopes: list[dict[str, tuple[str, str]]] = []
        self.local_serial = 0
        self.loop_depth = 0
        self.return_type = "void"
        self.literals: dict[str, str] = {}

    def error(self, message: str, line: int) -> None:
        raise PonaError(message, line, 1)

    def resolve_expr(self, expr: Expr) -> str:
        if expr.kind == "number":
            expr.type = "f32" if any(c in expr.value for c in ".eE") else "i32"
            if expr.type == "i32":
                if int(expr.value) > 2147483647:
                    self.error("целое число выходит за пределы i32", expr.line)
            elif not math.isfinite(float(expr.value)):
                self.error("число должно быть конечным", expr.line)
        elif expr.kind == "color":
            expr.type = "color"
        elif expr.kind == "string":
            expr.type = "str"
        elif expr.kind == "bool":
            expr.type = "bool"
        elif expr.kind == "nil":
            expr.type = "nil"
        elif expr.kind == "name":
            found = next((scope[expr.value] for scope in reversed(self.scopes) if expr.value in scope), None)
            if found:
                expr.type = found[1]
            elif expr.value in self.global_types:
                expr.type = self.global_types[expr.value]
            else:
                self.error(f"неизвестная переменная {expr.value!r}", expr.line)
        elif expr.kind == "call":
            args = expr.args or []
            if expr.value in BUILTINS:
                expected, result, _ = BUILTINS[expr.value]
                if len(args) != len(expected):
                    self.error(f"{expr.value} ожидает аргументов: {len(expected)}", expr.line)
                for arg, want in zip(args, expected):
                    got = self.resolve_expr(arg)
                    if not compatible(want, got):
                        self.error(f"{expr.value}: ожидался {want}, получен {got}", arg.line)
                expr.type = result
            elif expr.value in self.function_types:
                expected, result = self.function_types[expr.value]
                if len(args) != len(expected):
                    self.error(f"{expr.value} ожидает аргументов: {len(expected)}", expr.line)
                for arg, want in zip(args, expected):
                    got = self.resolve_expr(arg)
                    if not compatible(want, got):
                        self.error(f"{expr.value}: ожидался {want}, получен {got}", arg.line)
                expr.type = result
            else:
                self.error(f"неизвестная функция {expr.value!r}", expr.line)
        elif expr.kind == "unary":
            actual = self.resolve_expr(expr.left)
            if expr.value == "not":
                if actual != "bool": self.error("not применяется только к bool", expr.line)
                expr.type = "bool"
            else:
                if actual not in {"f32", "i32"}: self.error("унарный минус требует число", expr.line)
                expr.type = actual
        elif expr.kind == "binary":
            left, right = self.resolve_expr(expr.left), self.resolve_expr(expr.right)
            op = expr.value
            if op == "..":
                if left != "str" or right != "str": self.error("оператор .. соединяет строки", expr.line)
                expr.type = "str"
            elif op in {"and", "or"}:
                if left != "bool" or right != "bool": self.error(f"{op} применяется только к bool", expr.line)
                expr.type = "bool"
            elif op in {"==", "~=", "!="}:
                numeric = {left, right} <= {"f32", "i32"}
                nil_pointer = (left == "nil" and right in {"str", "array"}) or (
                    right == "nil" and left in {"str", "array"}
                )
                if left != right and not numeric and not nil_pointer:
                    self.error("сравнение требует совместимые типы", expr.line)
                expr.type = "bool"
            elif op in {"<", ">", "<=", ">="}:
                if left not in {"f32", "i32"} or right not in {"f32", "i32"}:
                    self.error("порядковое сравнение требует числа", expr.line)
                expr.type = "bool"
            else:
                if left not in {"f32", "i32"} or right not in {"f32", "i32"}:
                    self.error(f"оператор {op} требует числа", expr.line)
                expr.type = "f32" if op == "/" or "f32" in {left, right} else "i32"
        else:
            self.error("неизвестное выражение", expr.line)
        return expr.type

    def prepare_types(self) -> None:
        for function in self.program.functions:
            if function.name in BUILTINS:
                self.error(f"имя function {function.name!r} зарезервировано для встроенного API", function.line)
            if any(type_name == "void" for _, type_name in function.params):
                self.error("аргумент function не может иметь тип void", function.line)
            if function.name in {"init", "draw", "stop"} and (function.params or function.result != "void"):
                self.error(f"{function.name} должен иметь сигнатуру function {function.name}()", function.line)
            if function.name == "update" and (
                function.result != "void"
                or len(function.params) > 1
                or (function.params and function.params[0][1] != "f32")
            ):
                self.error("update должен иметь сигнатуру function update([delta: f32])", function.line)
        for global_ in self.program.globals:
            self.global_attrs[global_.name] = global_.attributes
            if global_.annotation:
                if global_.annotation == "void": self.error("global не может иметь тип void", global_.line)
                global_.type = global_.annotation
                self.global_types[global_.name] = global_.type
        for global_ in self.program.globals:
            self.scopes = []
            actual = self.resolve_expr(global_.expr)
            if global_.type == "?":
                if actual == "nil": self.error("тип global с nil нужно указать явно", global_.line)
                global_.type = actual
                self.global_types[global_.name] = actual
            elif not compatible(global_.type, actual):
                self.error(f"global {global_.name}: ожидался {global_.type}, получен {actual}", global_.line)
        for function in self.program.functions:
            if function.result == "void" and function.name in {"update", "draw", "init", "stop"}:
                continue

    def emit(self, line: str = "") -> None:
        self.lines.append("    " * self.indent + line if line else "")

    def global_c(self, name: str) -> str:
        return "pona_global_" + name

    def lookup(self, name: str, line: int) -> tuple[str, str, bool]:
        for scope in reversed(self.scopes):
            if name in scope:
                c_name, type_name = scope[name]
                return c_name, type_name, False
        if name in self.global_types:
            return self.global_c(name), self.global_types[name], True
        self.error(f"неизвестная переменная {name!r}", line)

    def expression_c(self, expr: Expr) -> str:
        if expr.kind == "number":
            value = expr.value
            if expr.type == "f32" and not value.lower().endswith("f"):
                value += "f"
            return value
        if expr.kind == "color": return f"UINT32_C(0x{expr.value:08X})"
        if expr.kind == "string": return self.literals[expr.value]
        if expr.kind == "bool": return "1" if expr.value else "0"
        if expr.kind == "nil": return "NULL"
        if expr.kind == "name": return self.lookup(expr.value, expr.line)[0]
        if expr.kind == "unary":
            op = "!" if expr.value == "not" else "-"
            return f"({op}{self.expression_c(expr.left)})"
        if expr.kind == "binary":
            left, right, op = self.expression_c(expr.left), self.expression_c(expr.right), expr.value
            if op == "..": return f"pona_string_concat({left}, {right})"
            if op in {"~=", "!="}:
                if expr.left.type == "str" and expr.right.type == "str":
                    return f"(!pona_string_equals({left}, {right}))"
                return f"({left} != {right})"
            if op == "==" and expr.left.type == "str" and expr.right.type == "str":
                return f"pona_string_equals({left}, {right})"
            if op == "/":
                return f"((float)({left}) / (float)({right}))"
            if op == "%":
                if expr.type == "f32": return f"fmodf((float)({left}), (float)({right}))"
                return f"({left} % {right})"
            c_op = {"and": "&&", "or": "||"}.get(op, op)
            return f"({left} {c_op} {right})"
        if expr.kind == "call":
            args = [self.expression_c(arg) for arg in (expr.args or [])]
            built = expr.value
            if built in BUILTINS:
                c_name = BUILTINS[built][2]
                if built == "text": args.append("0.5f")
                return f"{c_name}({', '.join(args)})"
            c_name = self.function_c_names[built]
            return f"{c_name}({', '.join(args)})"
        raise AssertionError(expr.kind)

    def push_scope(self) -> None:
        self.scopes.append({})

    def pop_scope(self) -> None:
        self.scopes.pop()

    def emit_statements(self, statements: list[Stmt], scoped: bool = True) -> None:
        if scoped: self.push_scope()
        for stmt in statements:
            self.emit_statement(stmt)
        if scoped: self.pop_scope()

    def emit_statement(self, stmt: Stmt) -> None:
        if stmt.kind == "local":
            actual = self.resolve_expr(stmt.expr)
            if actual == "nil" and not stmt.annotation:
                self.error("local с nil требует явной аннотации типа", stmt.line)
            type_name = stmt.annotation or actual
            if type_name == "void" or not compatible(type_name, actual):
                self.error(f"local {stmt.name}: несовместимый тип {actual}", stmt.line)
            if stmt.name in self.scopes[-1]: self.error(f"local {stmt.name} уже объявлена в этом блоке", stmt.line)
            c_name = f"pona_local_{self.local_serial}_{stmt.name}"
            self.local_serial += 1
            self.scopes[-1][stmt.name] = (c_name, type_name)
            self.emit(f"{c_type(type_name)} {c_name} = {self.expression_c(stmt.expr)};")
            return
        if stmt.kind == "assign":
            c_name, type_name, is_global = self.lookup(stmt.name, stmt.line)
            actual = self.resolve_expr(stmt.expr)
            if not compatible(type_name, actual): self.error(f"нельзя присвоить {actual} переменной типа {type_name}", stmt.line)
            if stmt.op != "=" and type_name not in {"f32", "i32"}:
                self.error("составное присваивание поддерживается только для чисел", stmt.line)
            if type_name in {"str", "array"} and is_global:
                self.emit(f"pona_gc_root_store(&{c_name}, {self.expression_c(stmt.expr)});")
            else:
                operator = stmt.op[:-1] if stmt.op != "=" else "="
                self.emit(f"{c_name} {operator}= {self.expression_c(stmt.expr)};" if stmt.op != "="
                          else f"{c_name} = {self.expression_c(stmt.expr)};")
            return
        if stmt.kind == "expr":
            self.resolve_expr(stmt.expr)
            self.emit(self.expression_c(stmt.expr) + ";")
            return
        if stmt.kind == "return":
            if stmt.expr is None:
                if self.return_type != "void": self.error("нужен результат return", stmt.line)
                self.emit("return;")
            else:
                actual = self.resolve_expr(stmt.expr)
                if self.return_type == "void" or not compatible(self.return_type, actual):
                    self.error(f"return должен иметь тип {self.return_type}", stmt.line)
                self.emit(f"return {self.expression_c(stmt.expr)};")
            return
        if stmt.kind == "break":
            if self.loop_depth == 0: self.error("break можно использовать только внутри цикла", stmt.line)
            self.emit("break;")
            return
        if stmt.kind == "if":
            if self.resolve_expr(stmt.expr) != "bool": self.error("условие if должно иметь тип bool", stmt.line)
            self.emit(f"if ({self.expression_c(stmt.expr)}) {{")
            self.indent += 1
            self.emit_statements(stmt.body or [])
            self.indent -= 1
            if stmt.otherwise:
                self.emit("} else {")
                self.indent += 1
                self.emit_statements(stmt.otherwise)
                self.indent -= 1
            self.emit("}")
            return
        if stmt.kind == "while":
            if self.resolve_expr(stmt.expr) != "bool": self.error("условие while должно иметь тип bool", stmt.line)
            self.emit(f"while ({self.expression_c(stmt.expr)}) {{")
            self.indent += 1
            self.loop_depth += 1
            self.emit_statements(stmt.body or [])
            self.loop_depth -= 1
            self.indent -= 1
            self.emit("}")
            return
        if stmt.kind == "for":
            start_type, stop_type, step_type = self.resolve_expr(stmt.start), self.resolve_expr(stmt.stop), self.resolve_expr(stmt.step)
            if any(t != "i32" for t in (start_type, stop_type, step_type)):
                self.error("границы for должны иметь тип i32", stmt.line)
            c_name = f"pona_local_{self.local_serial}_{stmt.name}"
            end_name, step_name = c_name + "_end", c_name + "_step"
            self.local_serial += 1
            self.emit("{")
            self.indent += 1
            self.emit(f"int64_t {c_name} = (int32_t)({self.expression_c(stmt.start)});")
            self.emit(f"int64_t {end_name} = (int32_t)({self.expression_c(stmt.stop)});")
            self.emit(f"int64_t {step_name} = (int32_t)({self.expression_c(stmt.step)});")
            self.emit(f"for (; {step_name} > 0 ? {c_name} <= {end_name} : ({step_name} < 0 && {c_name} >= {end_name}); {c_name} += {step_name}) {{")
            self.indent += 1
            self.push_scope()
            self.scopes[-1][stmt.name] = (c_name, "i32")
            self.loop_depth += 1
            self.emit_statements(stmt.body or [])
            self.loop_depth -= 1
            self.pop_scope()
            self.indent -= 1
            self.emit("}")
            self.indent -= 1
            self.emit("}")
            return
        raise AssertionError(stmt.kind)

    def emit_function(self, function: Function) -> None:
        self.return_type = function.result
        self.local_serial = 0
        params = ", ".join(f"{c_type(t)} pona_arg_{n}" for n, t in function.params) or "void"
        self.emit(f"{c_type(function.result)} {self.function_c_names[function.name]}({params}) {{")
        self.indent += 1
        self.push_scope()
        for name, type_name in function.params:
            self.scopes[-1][name] = ("pona_arg_" + name, type_name)
        self.emit_statements(function.body, scoped=False)
        if function.result != "void": self.emit("return 0;")
        self.pop_scope()
        self.indent -= 1
        self.emit("}")
        self.emit()

    def compile(self) -> str:
        self.prepare_types()
        self.collect_literals()
        self.emit("/* Generated by ponac. This file contains native C, not bytecode. */")
        self.emit('#include "engine2d/api.h"')
        self.emit('#include "engine2d/pona_runtime.h"')
        self.emit("#include <math.h>")
        self.emit("#include <stdint.h>")
        self.emit()
        for global_ in self.program.globals:
            self.emit(f"{c_type(global_.type)} {self.global_c(global_.name)};")
        for literal_name in self.literals.values():
            self.emit(f"PonaObject * {literal_name};")
        for function in self.program.functions:
            params = ", ".join(f"{c_type(t)} pona_arg_{n}" for n, t in function.params) or "void"
            self.emit(f"{c_type(function.result)} {self.function_c_names[function.name]}({params});")
        exported = [g for g in self.program.globals if "export" in g.attributes]
        self.emit("const PonaScriptExport pona_script_exports[] = {")
        self.indent += 1
        if exported:
            for global_ in exported:
                self.emit(f"{{{c_string(global_.name)}, {c_string(global_.type)}, &{self.global_c(global_.name)}}},")
        else:
            self.emit("{NULL, NULL, NULL},")
        self.indent -= 1
        self.emit("};")
        self.emit(f"const size_t pona_script_export_count = {len(exported)};")
        self.emit()
        for function in self.program.functions:
            self.emit_function(function)
        self.emit("void pona_script_init(void) {")
        self.indent += 1
        self.emit("pona_gc_init();")
        for value, literal_name in self.literals.items():
            self.emit(f"pona_gc_root_register(&{literal_name});")
            self.emit(f"pona_gc_root_store(&{literal_name}, pona_string_new({c_string(value)}));")
        for global_ in self.program.globals:
            if global_.type in {"str", "array"}:
                self.emit(f"pona_gc_root_register(&{self.global_c(global_.name)});")
        self.scopes = []
        for global_ in self.program.globals:
            value = self.expression_c(global_.expr)
            c_name = self.global_c(global_.name)
            if global_.type in {"str", "array"}:
                self.emit(f"pona_gc_root_store(&{c_name}, {value});")
            else:
                self.emit(f"{c_name} = {value};")
        init = self.function_c_names.get("init")
        if init: self.emit(f"{init}();")
        self.indent -= 1
        self.emit("}")
        self.emit("void pona_script_update(float delta) {")
        self.indent += 1
        update = self.function_c_names.get("update")
        if update:
            params = self._function("update").params
            self.emit(f"{update}(delta);" if params else f"{update}();")
        else:
            self.emit("(void)delta;")
        self.indent -= 1
        self.emit("}")
        self.emit("void pona_script_draw(void) {")
        self.indent += 1
        draw = self.function_c_names.get("draw")
        if draw: self.emit(f"{draw}();")
        self.indent -= 1
        self.emit("}")
        self.emit("void pona_script_stop(void) {")
        self.indent += 1
        stop = self.function_c_names.get("stop")
        if stop: self.emit(f"{stop}();")
        self.indent -= 1
        self.emit("}")
        return "\n".join(self.lines) + "\n"

    def _function(self, name: str) -> Function:
        return next(f for f in self.program.functions if f.name == name)

    def collect_literals(self) -> None:
        def visit_expr(expr: Expr | None) -> None:
            if expr is None: return
            if expr.kind == "string" and expr.value not in self.literals:
                self.literals[expr.value] = f"pona_literal_{len(self.literals)}"
            visit_expr(expr.left)
            visit_expr(expr.right)
            for arg in expr.args or []: visit_expr(arg)

        def visit_statements(statements: list[Stmt]) -> None:
            for stmt in statements:
                visit_expr(stmt.expr)
                visit_expr(stmt.start)
                visit_expr(stmt.stop)
                visit_expr(stmt.step)
                visit_statements(stmt.body or [])
                visit_statements(stmt.otherwise or [])

        for global_ in self.program.globals: visit_expr(global_.expr)
        for function in self.program.functions: visit_statements(function.body)


def compile_source(source: str, source_name: str = "<memory>") -> str:
    return Compiler(Parser(lex(source)).parse(), source_name).compile()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compile Pona source to native C (AOT).")
    parser.add_argument("source", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--check", action="store_true", help="parse and type-check without writing C")
    args = parser.parse_args(argv)
    try:
        text = args.source.read_text(encoding="utf-8")
        generated = compile_source(text, str(args.source))
        if not args.check:
            if not args.output:
                parser.error("укажи -o output.c или используй --check")
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(generated, encoding="utf-8")
            print(f"Pona AOT: {args.source} -> {args.output} (native C, no VM/bytecode)")
        else:
            print(f"Pona OK: {args.source} (AOT/native C)")
        return 0
    except (OSError, UnicodeError, PonaError) as error:
        print(f"ponac: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
