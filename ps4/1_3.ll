; Function Attrs: noinline nounwind uwtable
define dso_local i32 @my_factorial(i32 noundef %param) #0 {
    1:
        %iszerocmp = icmp eq i32 %param, 0
        br i1 %iszerocmp, label %X, label %2
    2:
        %islessthanzerocmp = icmp slt i32 %param, 0
        br i1 %islessthanzerocmp, label %X, label %3
    3:
        %.01 = phi i32 [ %inc, %5 ], [ 1, %2 ]
        %.0 = phi i32 [ %mul_result, %5 ], [ 1, %2 ]
    4:
        %continue = icmp sle i32 %.01, %param
        br i1 %continue, label %5 label %X
    5:
        %mul_result = mul nsw i32 %.0, %.01
        %inc = add nsw i32 %.01, 1
        br label %3
    6:
        %.2 = phi i32 [ -1, %1 ], [ -2, %2 ], [ %.0, %4 ]
        ret i32 %.2
}
