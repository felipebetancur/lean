/-
Copyright (c) Luke Nelson and Jared Roesch. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.
Authors: Luke Nelson and Jared Roesch
-/
prelude

structure functor [class] (f : Type → Type) : Type :=
(map : Π {a b: Type}, (a → b) → f a → f b)

inline definition fmap {A B : Type} {F : Type → Type} [functor F] (f : A → B) (a : F A) : F B :=
functor.map f a

infixr ` <$> `:100 := fmap