; ModuleID = 'testNotAdjacent.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [7 x i8] c"Test 1\00", align 1
@.str.1 = private unnamed_addr constant [7 x i8] c"Test 2\00", align 1

; Function Attrs: noinline nounwind uwtable
define dso_local void @adjacent_loops(i32 noundef %0) #0 {
  br label %2

2:                                                ; preds = %5, %1
  %.0 = phi i32 [ 0, %1 ], [ %6, %5 ]
  %3 = icmp slt i32 %.0, 100
  br i1 %3, label %4, label %7

4:                                                ; preds = %2
  br label %5

5:                                                ; preds = %4
  %6 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !6

7:                                                ; preds = %2
  %8 = icmp sgt i32 %0, 19
  br i1 %8, label %9, label %11

9:                                                ; preds = %7
  %10 = call i32 (ptr, ...) @printf(ptr noundef @.str)
  br label %13

11:                                               ; preds = %7
  %12 = call i32 (ptr, ...) @printf(ptr noundef @.str.1)
  br label %13

13:                                               ; preds = %11, %9
  br label %14

14:                                               ; preds = %17, %13
  %.1 = phi i32 [ 0, %13 ], [ %18, %17 ]
  %15 = icmp slt i32 %.1, 100
  br i1 %15, label %16, label %19

16:                                               ; preds = %14
  br label %17

17:                                               ; preds = %16
  %18 = add nsw i32 %.1, 1
  br label %14, !llvm.loop !8

19:                                               ; preds = %14
  ret void
}

declare i32 @printf(ptr noundef, ...) #1

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7 (/home/runner/work/llvm-project/llvm-project/clang cd708029e0b2869e80abe31ddb175f7c35361f90)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
