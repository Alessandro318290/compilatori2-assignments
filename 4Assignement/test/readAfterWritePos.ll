; ModuleID = 'readAfterWritePos.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @write_after_read_positive(ptr noalias noundef %0, ptr noalias noundef %1) #0 {
  br label %3

3:                                                ; preds = %10, %2
  %.0 = phi i32 [ 0, %2 ], [ %11, %10 ]
  %4 = icmp slt i32 %.0, 10
  br i1 %4, label %5, label %12

5:                                                ; preds = %3
  %6 = mul nsw i32 %.0, 10
  %7 = add nsw i32 %.0, 1
  %8 = sext i32 %7 to i64
  %9 = getelementptr inbounds i32, ptr %0, i64 %8
  store i32 %6, ptr %9, align 4
  br label %10

10:                                               ; preds = %5
  %11 = add nsw i32 %.0, 1
  br label %3, !llvm.loop !6

12:                                               ; preds = %3
  br label %13

13:                                               ; preds = %21, %12
  %.1 = phi i32 [ 0, %12 ], [ %22, %21 ]
  %14 = icmp slt i32 %.1, 10
  br i1 %14, label %15, label %23

15:                                               ; preds = %13
  %16 = sext i32 %.1 to i64
  %17 = getelementptr inbounds i32, ptr %0, i64 %16
  %18 = load i32, ptr %17, align 4
  %19 = sext i32 %.1 to i64
  %20 = getelementptr inbounds i32, ptr %1, i64 %19
  store i32 %18, ptr %20, align 4
  br label %21

21:                                               ; preds = %15
  %22 = add nsw i32 %.1, 1
  br label %13, !llvm.loop !8

23:                                               ; preds = %13
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
