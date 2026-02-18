# Classes, Structs, and Enums

Opus provides three ways to define custom data types: structs for plain data, classes for
data with behavior, and enums for named constants.

---

## Table of Contents

- [Structs](#structs)
- [Classes](#classes)
- [Enums](#enums)

---

## Structs

Structs are simple data containers. No methods, no inheritance, no vtables.

### Defining a Struct

#### C-Style Fields

```opus
struct Point {
    int x;
    int y;
}

struct Rect {
    int left;
    int top;
    int width;
    int height;
}
```

#### Rust-Style Fields

```opus
struct Point {
    x: int,
    y: int,
}
```

#### Mixed Syntax

Both field styles can coexist in the same struct:

```opus
struct Entity {
    int id;
    name: str,
    float speed;
    active: bool,
}
```

### Creating Instances (Struct Literals)

The compiler recognizes struct literals when the name starts with an uppercase letter
followed by braces:

```opus
let origin = Point { x: 0, y: 0 }
let player_pos = Point { x: 100, y: 200 }
let bounds = Rect { left: 0, top: 0, width: 800, height: 600 }
```

### Field Access

```opus
function int main() {
    alloc_console()
    let p = Point { x: 42, y: 99 }
    print_int(p.x)    // 42
    print_int(p.y)    // 99
    free(p)
    return 0
}
```

### Field Modification

```opus
function int main() {
    alloc_console()
    var p = Point { x: 10, y: 20 }
    p.x = 50
    p.y = 100
    print_int(p.x)    // 50
    free(p)
    return 0
}
```

### Compound Assignment on Fields

```opus
var p = Point { x: 10, y: 20 }
p.x += 5      // 15
p.y -= 3      // 17
```

### Memory Layout

- Every field occupies **8 bytes** regardless of declared type
- Structs are **heap-allocated** via `HeapAlloc`
- No automatic cleanup — call `free()` when done

```opus
// 3 fields = 24 bytes on the heap (3 x 8)
struct Vec3 {
    float x;
    float y;
    float z;
}

function int main() {
    alloc_console()
    let v = Vec3 { x: 1, y: 2, z: 3 }
    print_int(v.x)
    free(v)
    return 0
}
```

### Passing Structs to Functions

Struct variables are pointers under the hood, so passing them is cheap:

```opus
struct Player {
    int health;
    int score;
}

function void heal(ptr p, int amount) {
    p.health = p.health + amount
}

function int main() {
    alloc_console()
    var p = Player { health: 80, score: 500 }
    heal(p, 20)
    print_int(p.health)    // 100
    free(p)
    return 0
}
```

### Nested Structs

```opus
struct Inner { int value; }
struct Outer { ptr inner; int tag; }

function int main() {
    alloc_console()
    let i = Inner { value: 42 }
    let o = Outer { inner: i, tag: 1 }
    print_int(o.inner.value)    // chained field access: 42
    free(i)
    free(o)
    return 0
}
```

---

## Classes

Classes are structs with methods — data and behavior bundled together.

### Defining a Class

```opus
class Player {
    health: int,
    speed: int,
    name: str,

    function void takeDamage(int amount) {
        self.health = self.health - amount
        if self.health < 0 {
            self.health = 0
        }
    }

    function void heal(int amount) {
        self.health = self.health + amount
    }

    function int getHealth() {
        return self.health
    }

    function bool isAlive() {
        return self.health > 0
    }
}
```

Fields use the same syntax as structs. Methods use `function` (or `func`/`fn`) inside the
class body.

### Creating Instances

Same struct literal syntax:

```opus
var p = Player { health: 100, speed: 50, name: "hero" }
```

### Calling Methods

Dot syntax. The instance is passed implicitly as the first argument:

```opus
function int main() {
    alloc_console()
    var p = Player { health: 100, speed: 50, name: "hero" }

    p.takeDamage(30)
    print_int(p.getHealth())    // 70

    p.heal(10)
    print_int(p.getHealth())    // 80

    if p.isAlive() {
        print("still kicking\n")
    }

    free(p)
    return 0
}
```

### The `self` Keyword

Inside a method, `self` refers to the instance. Use it to read/write fields:

```opus
class Enemy {
    health: int,
    damage: int,
    alive: bool,

    function void attack(ptr target) {
        let dmg = self.damage
        target.health = target.health - dmg
    }

    function void die() {
        self.health = 0
        self.alive = false
    }

    function void update() {
        if self.health <= 0 {
            self.die()
        }
    }
}
```

### How Methods Compile

Methods become global functions with the class name prefixed. The instance pointer is the
first argument:

```opus
// this:
p.takeDamage(30)

// compiles to:
Player_takeDamage(p, 30)
```

You can call methods as regular functions too:

```opus
Player_takeDamage(p, 30)    // same thing
```

### Field Modification

```opus
var p = Player { health: 100, speed: 50, name: "hero" }

// direct assignment
p.health = 200
p.speed = 75

// compound assignment
p.health += 50
p.speed -= 10
```

### Practical Example: Game Entity

```opus
class Entity {
    int x;
    int y;
    int health;
    int speed;

    function void move(int dx, int dy) {
        self.x = self.x + dx * self.speed
        self.y = self.y + dy * self.speed
    }

    function void damage(int amount) {
        self.health = self.health - amount
        if self.health < 0 { self.health = 0 }
    }

    function bool isAlive() {
        return self.health > 0
    }

    function void print_status() {
        print("pos: (")
        print_int(self.x)
        print(", ")
        print_int(self.y)
        print(") hp: ")
        print_int(self.health)
        print("\n")
    }
}

function int main() {
    alloc_console()
    var e = Entity { x: 0, y: 0, health: 100, speed: 5 }
    e.move(1, 0)
    e.move(0, 1)
    e.damage(25)
    e.print_status()     // pos: (5, 5) hp: 75
    free(e)
    return 0
}
```

### Practical Example: Linked List

```opus
class Node {
    value: int,
    next: ptr,

    function void set_next(ptr node) {
        self.next = node
    }
}

function ptr make_list(int count) {
    var head = Node { value: 0, next: 0 }
    var current = head
    for i in range(1, count) {
        var node = Node { value: i, next: 0 }
        current.set_next(node)
        current = node
    }
    return head
}

function void print_list(ptr head) {
    var current = head
    while current != 0 {
        print_int(current.value)
        current = current.next
    }
}

function void free_list(ptr head) {
    var current = head
    while current != 0 {
        let next = current.next
        free(current)
        current = next
    }
}

function int main() {
    alloc_console()
    let list = make_list(5)
    print_list(list)       // 0 1 2 3 4
    free_list(list)
    return 0
}
```

---

## Enums

Enums define a set of named integer constants.

### Basic Definition

```opus
enum Direction {
    North,
    South,
    East,
    West,
}
```

Values auto-increment from 0: `North` = 0, `South` = 1, `East` = 2, `West` = 3.

### Explicit Values

Assign specific values. Subsequent variants continue from the last explicit value:

```opus
enum HttpStatus {
    OK = 200,
    Created,          // 201
    Accepted,         // 202
    BadRequest = 400,
    Unauthorized,     // 401
    Forbidden,        // 402
    NotFound = 404,
}
```

### Accessing Variants

Dot syntax with the enum name:

```opus
let dir = Direction.North
let status = HttpStatus.NotFound
```

### Using Enums in Comparisons

```opus
function void handle_direction(int dir) {
    if dir == Direction.North {
        print("going north\n")
    } else if dir == Direction.South {
        print("going south\n")
    } else if dir == Direction.East {
        print("going east\n")
    } else if dir == Direction.West {
        print("going west\n")
    }
}

function int main() {
    alloc_console()
    let dir = Direction.East
    handle_direction(dir)    // "going east"
    return 0
}
```

### Enums as Compile-Time Constants

Enum values are resolved at compile time and inlined directly. No runtime lookup —
`Direction.North` is just the number `0`.

### Practical Example: State Machine

```opus
enum State {
    Idle,
    Walking,
    Running,
    Jumping,
    Falling,
    Dead,
}

class Character {
    state: int,
    x: int,
    y: int,
    vy: int,

    function void update() {
        if self.state == State.Walking {
            self.x = self.x + 2
        } else if self.state == State.Running {
            self.x = self.x + 5
        } else if self.state == State.Jumping {
            self.y = self.y + self.vy
            self.vy = self.vy - 1
            if self.vy < 0 {
                self.state = State.Falling
            }
        } else if self.state == State.Falling {
            self.y = self.y + self.vy
            self.vy = self.vy - 1
            if self.y <= 0 {
                self.y = 0
                self.vy = 0
                self.state = State.Idle
            }
        }
    }

    function void jump() {
        if self.state == State.Idle or self.state == State.Walking or self.state == State.Running {
            self.state = State.Jumping
            self.vy = 10
        }
    }
}

function int main() {
    alloc_console()
    var c = Character { state: State.Idle, x: 0, y: 0, vy: 0 }

    c.state = State.Walking
    for i in range(0, 10) {
        c.update()
    }
    print("x after walking: ")
    print_int(c.x)    // 20

    c.jump()
    for i in range(0, 20) {
        c.update()
    }
    print("y after jump cycle: ")
    print_int(c.y)    // back to 0

    free(c)
    return 0
}
```

### Practical Example: Token Types

```opus
enum TokenKind {
    Ident,
    IntLit,
    StringLit,
    Plus,
    Minus,
    Star,
    Slash,
    Equals,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Semicolon,
    Eof,
}

struct Token {
    int kind;
    str text;
    int line;
}

function void print_token(ptr tok) {
    if tok.kind == TokenKind.Ident {
        print("IDENT")
    } else if tok.kind == TokenKind.IntLit {
        print("INT")
    } else if tok.kind == TokenKind.StringLit {
        print("STRING")
    } else if tok.kind == TokenKind.Eof {
        print("EOF")
    } else {
        print("OP")
    }
    print(": ")
    print(tok.text)
    print("\n")
}

function int main() {
    alloc_console()
    var tok = Token { kind: TokenKind.Ident, text: "hello", line: 1 }
    print_token(tok)    // IDENT: hello

    tok.kind = TokenKind.IntLit
    tok.text = "42"
    print_token(tok)    // INT: 42

    free(tok)
    return 0
}
```
