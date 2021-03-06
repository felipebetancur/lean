open nat

structure A [class] := (n : ℕ)

definition f [A] := A.n

structure B extends A :=
(Hf : f = 0)

example : B := ⦃B, n := 0, Hf := rfl⦄
