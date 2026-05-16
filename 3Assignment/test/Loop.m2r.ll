; ModuleID = 'Loop.ll'
source_filename = "Loop.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @loop(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  br label %4

4:                                                ; preds = %9, %3
  %.01 = phi i32 [ 0, %3 ], [ %10, %9 ]
  %.0 = phi i32 [ 0, %3 ], [ %8, %9 ]
  %5 = icmp slt i32 %.01, 10
  br i1 %5, label %6, label %11

6:                                                ; preds = %4
  %7 = mul nsw i32 5, 3
  %8 = add nsw i32 %.0, %7
  br label %9

9:                                                ; preds = %6
  %10 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !6

11:                                               ; preds = %4
  br label %12

12:                                               ; preds = %18, %11
  %.12 = phi i32 [ %0, %11 ], [ %19, %18 ]
  %.1 = phi i32 [ %.0, %11 ], [ %17, %18 ]
  %13 = icmp slt i32 %.12, %1
  br i1 %13, label %14, label %20

14:                                               ; preds = %12
  %15 = add nsw i32 5, 3
  %16 = mul nsw i32 %15, %.12
  %17 = add nsw i32 %.1, %16
  br label %18

18:                                               ; preds = %14
  %19 = add nsw i32 %.12, 1
  br label %12, !llvm.loop !8

20:                                               ; preds = %12
  ret i32 %.1
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.7 (++20250804090312+cd708029e0b2-1~exp1~20250804210325.79)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
