; ModuleID = '../test/test.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_non_fusible() #0 {
  %1 = alloca [10 x i32], align 16
  %2 = alloca [10 x i32], align 16
  br label %3

3:                                                ; preds = %8, %0
  %.0 = phi i32 [ 0, %0 ], [ %9, %8 ]
  %4 = icmp slt i32 %.0, 10
  br i1 %4, label %5, label %10

5:                                                ; preds = %3
  %6 = sext i32 %.0 to i64
  %7 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 %6
  store i32 %.0, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.0, 1
  br label %3, !llvm.loop !6

10:                                               ; preds = %3
  br label %11

11:                                               ; preds = %17, %10
  %.01 = phi i32 [ 0, %10 ], [ %18, %17 ]
  %12 = icmp slt i32 %.01, 20
  br i1 %12, label %13, label %19

13:                                               ; preds = %11
  %14 = mul nsw i32 %.01, 2
  %15 = sext i32 %.01 to i64
  %16 = getelementptr inbounds [10 x i32], ptr %2, i64 0, i64 %15
  store i32 %14, ptr %16, align 4
  br label %17

17:                                               ; preds = %13
  %18 = add nsw i32 %.01, 1
  br label %11, !llvm.loop !8

19:                                               ; preds = %11
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_non_fusible_dependency() #0 {
  %1 = alloca [10 x i32], align 16
  br label %2

2:                                                ; preds = %7, %0
  %.0 = phi i32 [ 0, %0 ], [ %8, %7 ]
  %3 = icmp slt i32 %.0, 10
  br i1 %3, label %4, label %9

4:                                                ; preds = %2
  %5 = sext i32 %.0 to i64
  %6 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 %5
  store i32 %.0, ptr %6, align 4
  br label %7

7:                                                ; preds = %4
  %8 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !9

9:                                                ; preds = %2
  br label %10

10:                                               ; preds = %17, %9
  %.01 = phi i32 [ 0, %9 ], [ %18, %17 ]
  %11 = icmp slt i32 %.01, 10
  br i1 %11, label %12, label %19

12:                                               ; preds = %10
  %13 = sext i32 %.01 to i64
  %14 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 %13
  %15 = load i32, ptr %14, align 4
  %16 = add nsw i32 %15, 1
  store i32 %16, ptr %14, align 4
  br label %17

17:                                               ; preds = %12
  %18 = add nsw i32 %.01, 1
  br label %10, !llvm.loop !10

19:                                               ; preds = %10
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_non_fusible_conditional() #0 {
  %1 = alloca [10 x i32], align 16
  br label %2

2:                                                ; preds = %7, %0
  %.0 = phi i32 [ 0, %0 ], [ %8, %7 ]
  %3 = icmp slt i32 %.0, 10
  br i1 %3, label %4, label %9

4:                                                ; preds = %2
  %5 = sext i32 %.0 to i64
  %6 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 %5
  store i32 %.0, ptr %6, align 4
  br label %7

7:                                                ; preds = %4
  %8 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !11

9:                                                ; preds = %2
  %10 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 0
  %11 = load i32, ptr %10, align 16
  %12 = icmp sgt i32 %11, 0
  br i1 %12, label %13, label %24

13:                                               ; preds = %9
  br label %14

14:                                               ; preds = %21, %13
  %.01 = phi i32 [ 0, %13 ], [ %22, %21 ]
  %15 = icmp slt i32 %.01, 10
  br i1 %15, label %16, label %23

16:                                               ; preds = %14
  %17 = sext i32 %.01 to i64
  %18 = getelementptr inbounds [10 x i32], ptr %1, i64 0, i64 %17
  %19 = load i32, ptr %18, align 4
  %20 = add nsw i32 %19, 1
  store i32 %20, ptr %18, align 4
  br label %21

21:                                               ; preds = %16
  %22 = add nsw i32 %.01, 1
  br label %14, !llvm.loop !12

23:                                               ; preds = %14
  br label %24

24:                                               ; preds = %23, %9
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @four_adjacent_loops() #0 {
  br label %1

1:                                                ; preds = %4, %0
  %.0 = phi i32 [ 0, %0 ], [ %5, %4 ]
  %2 = icmp slt i32 %.0, 100
  br i1 %2, label %3, label %6

3:                                                ; preds = %1
  br label %9

4:                                                ; preds = %9
  %5 = add nsw i32 %.0, 1
  br label %1, !llvm.loop !13

6:                                                ; preds = %1
  br label %7

7:                                                ; preds = %10, %6
  %.1 = phi i32 [ 0, %6 ], [ %11, %10 ]
  %8 = icmp slt i32 %.0, 100
  br i1 %8, label %9, label %12

9:                                                ; preds = %3, %7
  br label %4

10:                                               ; No predecessors!
  %11 = add nsw i32 %.0, 1
  br label %7, !llvm.loop !14

12:                                               ; preds = %7
  br label %13

13:                                               ; preds = %16, %12
  %.2 = phi i32 [ 0, %12 ], [ %17, %16 ]
  %14 = icmp slt i32 %.2, 10
  br i1 %14, label %15, label %18

15:                                               ; preds = %13
  br label %21

16:                                               ; preds = %21
  %17 = add nsw i32 %.2, 1
  br label %13, !llvm.loop !15

18:                                               ; preds = %13
  br label %19

19:                                               ; preds = %22, %18
  %.3 = phi i32 [ 0, %18 ], [ %23, %22 ]
  %20 = icmp slt i32 %.2, 10
  br i1 %20, label %21, label %24

21:                                               ; preds = %15, %19
  br label %16

22:                                               ; No predecessors!
  %23 = add nsw i32 %.2, 1
  br label %19, !llvm.loop !16

24:                                               ; preds = %19
  br label %25

25:                                               ; preds = %27, %24
  %.4 = phi i32 [ 0, %24 ], [ %26, %27 ]
  %26 = add nsw i32 %.4, 1
  br label %27

27:                                               ; preds = %30, %25
  %28 = icmp slt i32 %26, 10
  br i1 %28, label %25, label %29, !llvm.loop !17

29:                                               ; preds = %27
  br label %30

30:                                               ; preds = %32, %29
  %.01 = phi i32 [ 0, %29 ], [ %31, %32 ]
  %31 = add nsw i32 %.4, 1
  br label %27

32:                                               ; No predecessors!
  %33 = icmp slt i32 %31, 10
  br i1 %33, label %30, label %34, !llvm.loop !18

34:                                               ; preds = %32
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
!10 = distinct !{!10, !7}
!11 = distinct !{!11, !7}
!12 = distinct !{!12, !7}
!13 = distinct !{!13, !7}
!14 = distinct !{!14, !7}
!15 = distinct !{!15, !7}
!16 = distinct !{!16, !7}
!17 = distinct !{!17, !7}
!18 = distinct !{!18, !7}
