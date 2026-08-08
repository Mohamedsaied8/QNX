# Lab 02 — Passing arguments to threads

## Concept

`std::thread`'s constructor **copies (decays) each argument** into thread-local
storage before the new thread starts. This is a safety feature — the thread
owns its inputs and can't be tripped by the caller mutating them mid-flight. But
it surprises people: even if the target function takes `int&`, the thread binds
that reference to its *own copy*. Use `std::ref` / `std::cref` to pass a real
reference, and `std::move` to transfer ownership of a move-only argument.

## Task

The starter runs an `Accumulator` functor on a thread, but `acc.total` stays 0
because the thread got a *copy* of the object. Wrap it in `std::ref` so the real
object is updated (result `4950`).

> Note: the copy applies to the **callable object too**, not just the arguments
> — that's why the functor's state is lost. The same `std::ref` fix applies to
> reference *arguments*: `std::thread t(f, std::ref(value))` for `void f(int&)`.
> (Try `std::thread t(f, value)` with `void f(int&)` and you'll get a *compile*
> error — the library refuses to silently bind a reference to a temporary copy.)

## Expected output

```
acc.total = 4950  (expected 4950)   # solution
acc.total = 0     (expected 4950)   # starter (the bug)
```

## Gotchas to discuss

- `std::ref` is a loaded gun once you `detach()` — the thread can outlive the
  referent. See Lab 03.
- For move-only arguments (e.g. `std::unique_ptr`, `std::promise`), pass
  `std::move(x)`.
- A common mistake: `std::thread t(f, std::ref(x))` where `f` takes `const&` —
  use `std::cref(x)`.

## QNX / RTOS note

The same copy-semantics apply with raw `pthread_create`, except *there* you pass
a single `void*` and are responsible for the lifetime and casting yourself —
exactly the dangling-pointer hazard this lab warns about, but with no type
safety. `std::thread`'s argument forwarding is one of the main reasons to prefer
it over the raw POSIX call in portable code.
