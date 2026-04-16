; ModuleID = '../test/test.ll'
source_filename = "../test/test.ll"

define dso_local i32 @foo() {
entry:
  %a0 = add i32 0, 5
  %b0 = add i32 0, 0
  %a1 = sub i32 %b0, 1
  ret i32 %b0
}
