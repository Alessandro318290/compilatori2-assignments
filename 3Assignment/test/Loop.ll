; ModuleID = 'Loop.c'
source_filename = "Loop.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @loop(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  %9 = alloca i32, align 4
  %10 = alloca i32, align 4
  %11 = alloca i32, align 4
  %12 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  store i32 %2, ptr %6, align 4
  store i32 0, ptr %8, align 4
  store i32 5, ptr %9, align 4
  store i32 3, ptr %10, align 4
  store i32 0, ptr %7, align 4
  br label %13

13:                                               ; preds = %23, %3
  %14 = load i32, ptr %7, align 4
  %15 = icmp slt i32 %14, 10
  br i1 %15, label %16, label %26

16:                                               ; preds = %13
  %17 = load i32, ptr %9, align 4
  %18 = load i32, ptr %10, align 4
  %19 = mul nsw i32 %17, %18
  store i32 %19, ptr %11, align 4
  %20 = load i32, ptr %8, align 4
  %21 = load i32, ptr %11, align 4
  %22 = add nsw i32 %20, %21
  store i32 %22, ptr %8, align 4
  br label %23

23:                                               ; preds = %16
  %24 = load i32, ptr %7, align 4
  %25 = add nsw i32 %24, 1
  store i32 %25, ptr %7, align 4
  br label %13, !llvm.loop !6

26:                                               ; preds = %13
  %27 = load i32, ptr %4, align 4
  store i32 %27, ptr %7, align 4
  br label %28

28:                                               ; preds = %41, %26
  %29 = load i32, ptr %7, align 4
  %30 = load i32, ptr %5, align 4
  %31 = icmp slt i32 %29, %30
  br i1 %31, label %32, label %44

32:                                               ; preds = %28
  %33 = load i32, ptr %9, align 4
  %34 = load i32, ptr %10, align 4
  %35 = add nsw i32 %33, %34
  store i32 %35, ptr %12, align 4
  %36 = load i32, ptr %12, align 4
  %37 = load i32, ptr %7, align 4
  %38 = mul nsw i32 %36, %37
  %39 = load i32, ptr %8, align 4
  %40 = add nsw i32 %39, %38
  store i32 %40, ptr %8, align 4
  br label %41

41:                                               ; preds = %32
  %42 = load i32, ptr %7, align 4
  %43 = add nsw i32 %42, 1
  store i32 %43, ptr %7, align 4
  br label %28, !llvm.loop !8

44:                                               ; preds = %28
  %45 = load i32, ptr %8, align 4
  ret i32 %45
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
