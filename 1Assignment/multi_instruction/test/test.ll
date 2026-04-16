define dso_local i32 @foo() #0 {
entry:
  %a0 = add i32 0, 5      ; a = 5
  %b0 = add i32 0, 0      ; b = 0

  %a1 = sub i32 %b0, 1    ; a = b + 1
  %c0 = add i32 %a1, 1    ; c = a - 1

  ret i32 %c0
}
