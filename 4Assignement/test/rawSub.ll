; ModuleID = 'rawSub.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @read_after_write_subloop(ptr noalias noundef %0, ptr noalias noundef %1) #0 {
  br label %3

3:                                                ; preds = %25, %2
  %.01 = phi i32 [ 0, %2 ], [ %26, %25 ]
  %4 = icmp slt i32 %.01, 10
  br i1 %4, label %5, label %27

5:                                                ; preds = %3
  br label %6

6:                                                ; preds = %11, %5
  %.0 = phi i32 [ 0, %5 ], [ %12, %11 ]
  %7 = icmp slt i32 %.0, 10
  br i1 %7, label %8, label %13

8:                                                ; preds = %6
  %9 = sext i32 %.0 to i64
  %10 = getelementptr inbounds i32, ptr %0, i64 %9
  store i32 %.0, ptr %10, align 4
  br label %11

11:                                               ; preds = %8
  %12 = add nsw i32 %.0, 1
  br label %6, !llvm.loop !6

13:                                               ; preds = %6
  br label %14

14:                                               ; preds = %22, %13
  %.1 = phi i32 [ 0, %13 ], [ %23, %22 ]
  %15 = icmp slt i32 %.1, 10
  br i1 %15, label %16, label %24

16:                                               ; preds = %14
  %17 = sext i32 %.1 to i64
  %18 = getelementptr inbounds i32, ptr %0, i64 %17
  %19 = load i32, ptr %18, align 4
  %20 = sext i32 %.1 to i64
  %21 = getelementptr inbounds i32, ptr %1, i64 %20
  store i32 %19, ptr %21, align 4
  br label %22

22:                                               ; preds = %16
  %23 = add nsw i32 %.1, 1
  br label %14, !llvm.loop !8

24:                                               ; preds = %14
  br label %25

25:                                               ; preds = %24
  %26 = add nsw i32 %.01, 1
  br label %3, !llvm.loop !9

27:                                               ; preds = %3
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

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
!9 = distinct !{!9, !7}
