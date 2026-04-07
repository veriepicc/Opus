# Structs, Classes, and Enums

This page reflects the current AST/parser surface.

## Structs

Field declarations support both common styles:

```c
struct Point {
    int x
    y: int
}
```

Struct literals:

```c
let p = Point { x: 10, y: 20 }
```

Field shorthand also works:

```c
let left = 10
let right = 20
let pair = Pair { left, right }
```

Nested field access works:

```c
let rect = Rect { origin: p, width: 100, height: 50 }
print_int(rect.origin.x)
```

## Classes

Classes are data plus methods. Fields use the same syntax as structs.

```c
class Counter {
    int value
    name: str
}
```

Methods currently use an implicit `self` receiver.

Supported method forms:

```c
class Counter {
    int value

    fn bump(delta: int) -> int => self.value + delta

    function int mix(int a, int b) {
        return self.value + a + b
    }

    int raw(int x) => self.value + x
}
```

Create instances with the same literal surface:

```c
let counter = Counter { value: 7, name: "demo" }
```

Call methods normally:

```c
let result = counter.bump(3)
```

## Mutation

Field mutation works through handle-like values:

```c
var counter = Counter { value: 1, name: "x" }
counter.value = counter.value + 1
```

## Enums

Enums are named integral constants:

```c
enum State {
    Idle,
    Running,
    Dead,
}
```

Explicit values also work:

```c
enum Op {
    Add = 1,
    Sub = 2,
}
```

Use them with dotted names:

```c
let state = State.Idle
if state == State.Idle {
    print("idle\n")
}
```

## What this page does not promise

Current classes are intentionally simple:

- no inheritance
- no overload sets
- no pattern-matching enums
- no full property system

The current goal is a clean, predictable object/data surface that matches codegen and tests.
