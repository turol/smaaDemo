; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 1
; Bound: 25140
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %5663 "main" %3809 %3810 %3811 %3569 %4945 %4402
               OpExecutionMode %5663 OriginUpperLeft
               OpMemberDecorate %_struct_1117 0 Offset 0
               OpMemberDecorate %_struct_1117 1 Offset 4
               OpMemberDecorate %_struct_1117 2 Offset 8
               OpMemberDecorate %_struct_1117 3 Offset 12
               OpMemberDecorate %_struct_1117 4 Offset 16
               OpMemberDecorate %_struct_1117 5 Offset 20
               OpMemberDecorate %_struct_1117 6 Offset 24
               OpMemberDecorate %_struct_1117 7 Offset 28
               OpMemberDecorate %_struct_1341 0 Offset 0
               OpMemberDecorate %_struct_1341 1 ColMajor
               OpMemberDecorate %_struct_1341 1 Offset 16
               OpMemberDecorate %_struct_1341 1 MatrixStride 16
               OpMemberDecorate %_struct_1341 2 ColMajor
               OpMemberDecorate %_struct_1341 2 Offset 80
               OpMemberDecorate %_struct_1341 2 MatrixStride 16
               OpMemberDecorate %_struct_1341 3 Offset 144
               OpMemberDecorate %_struct_1341 4 Offset 176
               OpMemberDecorate %_struct_1341 5 Offset 180
               OpMemberDecorate %_struct_1341 6 Offset 184
               OpMemberDecorate %_struct_1341 7 Offset 188
               OpDecorate %_struct_1341 Block
               OpDecorate %4930 DescriptorSet 0
               OpDecorate %4930 Binding 0
               OpDecorate %3809 Location 2
               OpDecorate %3810 Location 3
               OpDecorate %3811 Location 4
               OpDecorate %3569 Location 0
               OpDecorate %4945 Location 0
               OpDecorate %4402 Location 1
               OpDecorate %3899 DescriptorSet 1
               OpDecorate %3899 Binding 0
               OpDecorate %4961 DescriptorSet 1
               OpDecorate %4961 Binding 1
               OpDecorate %5463 DescriptorSet 1
               OpDecorate %5463 Binding 2
       %void = OpTypeVoid
       %1282 = OpTypeFunction %void
       %bool = OpTypeBool
     %v2bool = OpTypeVector %bool 2
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v4float = OpTypeVector %float 4
        %150 = OpTypeImage %float 2D 0 0 0 1 Unknown
        %510 = OpTypeSampledImage %150
%_ptr_UniformConstant_510 = OpTypePointer UniformConstant %510
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
    %float_5 = OpConstant %float 5
 %float_3_75 = OpConstant %float 3.75
   %float_n1 = OpConstant %float -1
    %float_1 = OpConstant %float 1
    %v3float = OpTypeVector %float 3
%mat4v4float = OpTypeMatrix %v4float 4
%_struct_1117 = OpTypeStruct %float %float %uint %uint %uint %uint %uint %uint
%_struct_1341 = OpTypeStruct %v4float %mat4v4float %mat4v4float %_struct_1117 %float %float %float %float
%_ptr_Uniform__struct_1341 = OpTypePointer Uniform %_struct_1341
       %4930 = OpVariable %_ptr_Uniform__struct_1341 Uniform
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
   %float_15 = OpConstant %float 15
  %float_0_9 = OpConstant %float 0.9
    %float_0 = OpConstant %float 0
  %float_0_5 = OpConstant %float 0.5
       %1566 = OpConstantComposite %v2float %float_0_5 %float_0_5
 %float_0_25 = OpConstant %float 0.25
%_ptr_Uniform_float = OpTypePointer Uniform %float
   %float_20 = OpConstant %float 20
       %1893 = OpConstantComposite %v2float %float_20 %float_20
%float_0_00625 = OpConstant %float 0.00625
%float_0_00178571 = OpConstant %float 0.00178571
        %674 = OpConstantComposite %v2float %float_0_00625 %float_0_00178571
%float_0_003125 = OpConstant %float 0.003125
%float_0_000892857 = OpConstant %float 0.000892857
       %1472 = OpConstantComposite %v2float %float_0_003125 %float_0_000892857
%float_0_142857 = OpConstant %float 0.142857
       %1823 = OpConstantComposite %v2float %float_0 %float_0
    %float_2 = OpConstant %float 2
      %v2int = OpTypeVector %int 2
     %int_n1 = OpConstant %int -1
       %1806 = OpConstantComposite %v2int %int_n1 %int_0
      %int_1 = OpConstant %int 1
       %1824 = OpConstantComposite %v2int %int_1 %int_0
       %2981 = OpConstantComposite %v2float %float_2 %float_2
        %768 = OpConstantComposite %v2float %float_1 %float_1
       %1827 = OpConstantComposite %v2int %int_0 %int_1
        %889 = OpConstantComposite %v2float %float_0 %float_1
%float_0_8281 = OpConstant %float 0.8281
   %float_n2 = OpConstant %float -2
   %float_n0 = OpConstant %float -0
       %2286 = OpConstantComposite %v2float %float_n2 %float_n0
%float_n2_00787 = OpConstant %float -2.00787
 %float_3_25 = OpConstant %float 3.25
       %2982 = OpConstantComposite %v2float %float_2 %float_0
        %312 = OpConstantComposite %v2float %float_1 %float_0
       %1128 = OpConstantComposite %v2float %float_n0 %float_2
       %1825 = OpConstantComposite %v2float %float_0 %float_n2
   %float_16 = OpConstant %float 16
        %587 = OpConstantComposite %v2float %float_16 %float_16
    %float_4 = OpConstant %float 4
 %float_0_75 = OpConstant %float 0.75
       %1803 = OpConstantComposite %v2int %int_0 %int_n1
       %1812 = OpConstantComposite %v2int %int_1 %int_n1
      %int_2 = OpConstant %int 2
       %1839 = OpConstantComposite %v2int %int_0 %int_2
       %1848 = OpConstantComposite %v2int %int_1 %int_2
     %int_n2 = OpConstant %int -2
       %1797 = OpConstantComposite %v2int %int_n2 %int_0
       %1785 = OpConstantComposite %v2int %int_n2 %int_n1
       %2938 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%_ptr_Input_v4float = OpTypePointer Input %v4float
       %3809 = OpVariable %_ptr_Input_v4float Input
       %3810 = OpVariable %_ptr_Input_v4float Input
       %3811 = OpVariable %_ptr_Input_v4float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %3569 = OpVariable %_ptr_Output_v4float Output
%_ptr_Input_v2float = OpTypePointer Input %v2float
       %4945 = OpVariable %_ptr_Input_v2float Input
       %4402 = OpVariable %_ptr_Input_v2float Input
       %3899 = OpVariable %_ptr_UniformConstant_510 UniformConstant
       %4961 = OpVariable %_ptr_UniformConstant_510 UniformConstant
       %5463 = OpVariable %_ptr_UniformConstant_510 UniformConstant
      %16660 = OpUndef %v4float
      %12938 = OpUndef %v2float
      %24831 = OpUndef %v3float
      %25127 = OpConstantComposite %v2float %float_0_5 %float_n2
%float_0_0078125 = OpConstant %float 0.0078125
%float_2_03125 = OpConstant %float 2.03125
      %25130 = OpConstantComposite %v2float %float_0_0078125 %float_2_03125
      %25131 = OpConstantComposite %v2float %float_3_75 %float_3_75
      %25132 = OpConstantComposite %v2float %float_0_9 %float_0_9
%float_0_523438 = OpConstant %float 0.523438
      %25138 = OpConstantComposite %v2float %float_0_523438 %float_2_03125
%float_n0_25 = OpConstant %float -0.25
       %5663 = OpFunction %void None %1282
      %24607 = OpLabel
      %20754 = OpLoad %v4float %3809
      %23772 = OpLoad %v4float %3810
      %11482 = OpLoad %v4float %3811
      %11483 = OpLoad %v2float %4945
      %11976 = OpLoad %v2float %4402
       %7326 = OpLoad %510 %3899
      %24582 = OpImageSampleImplicitLod %v4float %7326 %11483
       %8417 = OpVectorShuffle %v2float %24582 %24582 0 1
      %17057 = OpCompositeExtract %float %24582 1
      %10155 = OpFOrdGreaterThan %bool %17057 %float_0
               OpSelectionMerge %21263 None
               OpBranchConditional %10155 %11736 %21263
      %11736 = OpLabel
      %21523 = OpCompositeExtract %float %24582 0
      %14385 = OpFOrdGreaterThan %bool %21523 %float_0
               OpSelectionMerge %13982 None
               OpBranchConditional %14385 %20854 %12524
      %20854 = OpLabel
      %22005 = OpFNegate %float %float_1
      %10518 = OpCompositeExtract %float %11483 0
      %12343 = OpCompositeExtract %float %11483 1
      %18241 = OpCompositeConstruct %v4float %10518 %12343 %float_n1 %float_1
       %8922 = OpAccessChain %_ptr_Uniform_v4float %4930 %int_0
       %6933 = OpLoad %v4float %8922
      %23994 = OpCompositeExtract %float %6933 0
       %9412 = OpCompositeExtract %float %6933 1
       %9033 = OpCompositeConstruct %v3float %23994 %9412 %float_1
               OpBranch %20259
      %20259 = OpLabel
      %11194 = OpPhi %v2float %12938 %20854 %15275 %8263
      %14247 = OpPhi %v4float %18241 %20854 %10993 %8263
               OpLoopMerge %14333 %8263 None
               OpBranch %14369
      %14369 = OpLabel
      %24291 = OpCompositeExtract %float %14247 2
      %11700 = OpFOrdLessThan %bool %24291 %float_15
               OpSelectionMerge %15745 None
               OpBranchConditional %11700 %11737 %15745
      %11737 = OpLabel
      %21561 = OpCompositeExtract %float %14247 3
      %14001 = OpFOrdGreaterThan %bool %21561 %float_0_9
               OpBranch %15745
      %15745 = OpLabel
      %10367 = OpPhi %bool %11700 %14369 %14001 %11737
               OpBranchConditional %10367 %8263 %14333
       %8263 = OpLabel
      %24741 = OpCompositeConstruct %v3float %float_n1 %22005 %float_1
      %18281 = OpVectorShuffle %v3float %14247 %14247 0 1 2
      %16025 = OpExtInst %v3float %1 Fma %9033 %24741 %18281
      %20460 = OpVectorShuffle %v4float %14247 %16025 4 5 6 3
       %9471 = OpVectorShuffle %v2float %20460 %20460 0 1
       %8026 = OpImageSampleExplicitLod %v4float %7326 %9471 Lod %float_0
      %15275 = OpVectorShuffle %v2float %8026 %8026 0 1
      %11265 = OpDot %float %15275 %1566
      %10993 = OpCompositeInsert %v4float %11265 %20460 3
               OpBranch %20259
      %14333 = OpLabel
      %17546 = OpVectorShuffle %v2float %14247 %14247 2 3
      %16542 = OpVectorShuffle %v4float %16660 %17546 4 1 5 3
       %9123 = OpCompositeExtract %float %11194 1
      %20438 = OpFOrdGreaterThan %bool %9123 %float_0_9
      %21883 = OpSelect %float %20438 %float_1 %float_0
      %17775 = OpCompositeExtract %float %14247 0
      %24308 = OpFAdd %float %17775 %21883
      %24094 = OpCompositeInsert %v4float %24308 %16542 0
               OpBranch %13982
      %12524 = OpLabel
      %10380 = OpVectorShuffle %v4float %16660 %1823 4 1 5 3
               OpBranch %13982
      %13982 = OpLabel
       %9402 = OpPhi %v4float %24094 %14333 %10380 %12524
       %8525 = OpFNegate %float %float_n1
      %11327 = OpCompositeExtract %float %11483 0
      %12344 = OpCompositeExtract %float %11483 1
      %18242 = OpCompositeConstruct %v4float %11327 %12344 %float_n1 %float_1
       %8923 = OpAccessChain %_ptr_Uniform_v4float %4930 %int_0
       %6934 = OpLoad %v4float %8923
      %16723 = OpVectorShuffle %v2float %6934 %6934 0 1
      %23995 = OpCompositeExtract %float %6934 0
       %9413 = OpCompositeExtract %float %6934 1
       %9034 = OpCompositeConstruct %v3float %23995 %9413 %float_1
               OpBranch %20278
      %20278 = OpLabel
      %11078 = OpPhi %v4float %18242 %13982 %10994 %8264
               OpLoopMerge %14334 %8264 None
               OpBranch %14370
      %14370 = OpLabel
      %24292 = OpCompositeExtract %float %11078 2
      %11701 = OpFOrdLessThan %bool %24292 %float_15
               OpSelectionMerge %15746 None
               OpBranchConditional %11701 %11738 %15746
      %11738 = OpLabel
      %21562 = OpCompositeExtract %float %11078 3
      %14002 = OpFOrdGreaterThan %bool %21562 %float_0_9
               OpBranch %15746
      %15746 = OpLabel
      %10368 = OpPhi %bool %11701 %14370 %14002 %11738
               OpBranchConditional %10368 %8264 %14334
       %8264 = OpLabel
      %24742 = OpCompositeConstruct %v3float %float_1 %8525 %float_1
      %18282 = OpVectorShuffle %v3float %11078 %11078 0 1 2
      %16026 = OpExtInst %v3float %1 Fma %9034 %24742 %18282
      %20461 = OpVectorShuffle %v4float %11078 %16026 4 5 6 3
       %9472 = OpVectorShuffle %v2float %20461 %20461 0 1
       %8027 = OpImageSampleExplicitLod %v4float %7326 %9472 Lod %float_0
      %15276 = OpVectorShuffle %v2float %8027 %8027 0 1
      %11266 = OpDot %float %15276 %1566
      %10994 = OpCompositeInsert %v4float %11266 %20461 3
               OpBranch %20278
      %14334 = OpLabel
      %17547 = OpVectorShuffle %v2float %11078 %11078 2 3
      %14547 = OpVectorShuffle %v4float %9402 %17547 0 4 2 5
       %7457 = OpCompositeExtract %float %9402 0
      %18809 = OpCompositeExtract %float %11078 0
      %12490 = OpFAdd %float %7457 %18809
      %24144 = OpFOrdGreaterThan %bool %12490 %float_2
               OpSelectionMerge %20844 None
               OpBranchConditional %24144 %21766 %20844
      %21766 = OpLabel
      %14575 = OpFNegate %float %7457
      %23370 = OpFSub %float %float_0_25 %7457
       %9958 = OpFSub %float %18809 %float_n0_25
      %18004 = OpCompositeConstruct %v4float %23370 %14575 %18809 %9958
       %9124 = OpVectorShuffle %v4float %6934 %6934 0 1 0 1
      %18335 = OpVectorShuffle %v4float %11483 %11483 0 1 0 1
      %12161 = OpExtInst %v4float %1 Fma %18004 %9124 %18335
      %18732 = OpVectorShuffle %v2float %12161 %12161 0 1
      %13575 = OpImageSampleExplicitLod %v4float %7326 %18732 Lod|ConstOffset %float_0 %1806
       %8084 = OpVectorShuffle %v2float %13575 %13575 0 1
       %8369 = OpVectorShuffle %v4float %16660 %8084 4 5 2 3
      %21682 = OpVectorShuffle %v2float %12161 %12161 2 3
       %6715 = OpImageSampleExplicitLod %v4float %7326 %21682 Lod|ConstOffset %float_0 %1824
       %8085 = OpVectorShuffle %v2float %6715 %6715 0 1
       %9395 = OpVectorShuffle %v4float %8369 %8085 0 1 4 5
      %12359 = OpVectorShuffle %v2float %9395 %9395 0 2
      %22812 = OpVectorTimesScalar %v2float %12359 %float_5
      %24887 = OpFSub %v2float %22812 %25131
      %13406 = OpExtInst %v2float %1 FAbs %24887
      %22117 = OpFMul %v2float %12359 %13406
       %8003 = OpVectorShuffle %v4float %9395 %22117 4 1 5 3
      %24279 = OpExtInst %v4float %1 Round %8003
      %11684 = OpVectorShuffle %v4float %9395 %24279 5 4 7 6
      %13546 = OpVectorShuffle %v2float %11684 %11684 0 2
      %14300 = OpVectorShuffle %v2float %11684 %11684 1 3
      %12009 = OpExtInst %v2float %1 Fma %2981 %13546 %14300
      %18891 = OpVectorShuffle %v2float %14547 %14547 2 3
      %12613 = OpExtInst %v2float %1 Step %25132 %18891
      %24678 = OpFOrdNotEqual %v2bool %12613 %1823
      %22788 = OpCompositeExtract %bool %24678 0
               OpSelectionMerge %18756 None
               OpBranchConditional %22788 %12760 %18756
      %12760 = OpLabel
      %21521 = OpCompositeInsert %v2float %float_0 %12009 0
               OpBranch %18756
      %18756 = OpLabel
      %20514 = OpPhi %v2float %12009 %21766 %21521 %12760
      %16618 = OpCompositeExtract %bool %24678 1
               OpSelectionMerge %18718 None
               OpBranchConditional %16618 %12761 %18718
      %12761 = OpLabel
      %21522 = OpCompositeInsert %v2float %float_0 %20514 1
               OpBranch %18718
      %18718 = OpLabel
      %16445 = OpPhi %v2float %20514 %18756 %21522 %12761
      %10515 = OpVectorShuffle %v2float %14547 %14547 0 1
       %8128 = OpExtInst %v2float %1 Fma %1893 %16445 %10515
       %9644 = OpExtInst %v2float %1 Fma %674 %8128 %1472
      %21241 = OpCompositeExtract %float %9644 0
      %14492 = OpFAdd %float %21241 %float_0_5
      %21918 = OpCompositeInsert %v2float %14492 %9644 0
       %9264 = OpCompositeExtract %float %9644 1
      %10324 = OpFSub %float %float_1 %9264
      %21365 = OpCompositeInsert %v2float %10324 %21918 1
       %6423 = OpLoad %510 %4961
      %18095 = OpImageSampleExplicitLod %v4float %6423 %21365 Lod %float_0
      %20132 = OpVectorShuffle %v2float %18095 %18095 0 1
               OpBranch %20844
      %20844 = OpLabel
      %19748 = OpPhi %v2float %1823 %14334 %20132 %18718
      %25093 = OpAccessChain %_ptr_Uniform_float %4930 %int_0 %uint_0
      %25035 = OpLoad %float %25093
      %17262 = OpFMul %float %float_0_25 %25035
      %19613 = OpFAdd %float %11327 %17262
      %14676 = OpCompositeInsert %v4float %19613 %18242 0
               OpBranch %21891
      %21891 = OpLabel
      %11079 = OpPhi %v4float %14676 %20844 %21594 %8265
               OpLoopMerge %14335 %8265 None
               OpBranch %14371
      %14371 = OpLabel
      %24293 = OpCompositeExtract %float %11079 2
      %11702 = OpFOrdLessThan %bool %24293 %float_15
               OpSelectionMerge %15747 None
               OpBranchConditional %11702 %11739 %15747
      %11739 = OpLabel
      %21563 = OpCompositeExtract %float %11079 3
      %14003 = OpFOrdGreaterThan %bool %21563 %float_0_9
               OpBranch %15747
      %15747 = OpLabel
      %10369 = OpPhi %bool %11702 %14371 %14003 %11739
               OpBranchConditional %10369 %8265 %14335
       %8265 = OpLabel
      %24743 = OpCompositeConstruct %v3float %float_n1 %8525 %float_1
      %18283 = OpVectorShuffle %v3float %11079 %11079 0 1 2
      %16027 = OpExtInst %v3float %1 Fma %9034 %24743 %18283
      %20462 = OpVectorShuffle %v4float %11079 %16027 4 5 6 3
       %9473 = OpVectorShuffle %v2float %20462 %20462 0 1
       %6753 = OpImageSampleExplicitLod %v4float %7326 %9473 Lod %float_0
       %8764 = OpVectorShuffle %v2float %6753 %6753 0 1
      %21600 = OpCompositeExtract %float %6753 0
      %21301 = OpFMul %float %float_5 %21600
      %22335 = OpFSub %float %21301 %float_3_75
      %20694 = OpExtInst %float %1 FAbs %22335
      %21598 = OpFMul %float %21600 %20694
      %22877 = OpCompositeInsert %v2float %21598 %8764 0
      %11652 = OpExtInst %v2float %1 Round %22877
      %13942 = OpDot %float %11652 %1566
      %21594 = OpCompositeInsert %v4float %13942 %20462 3
               OpBranch %21891
      %14335 = OpLabel
      %17679 = OpVectorShuffle %v2float %11079 %11079 2 3
      %13336 = OpVectorShuffle %v4float %14547 %17679 4 1 5 3
       %8402 = OpImageSampleExplicitLod %v4float %7326 %11483 Lod|ConstOffset %float_0 %1824
      %14521 = OpCompositeExtract %float %8402 0
      %14185 = OpFOrdGreaterThan %bool %14521 %float_0
               OpSelectionMerge %13108 None
               OpBranchConditional %14185 %24046 %12525
      %24046 = OpLabel
      %15185 = OpFNegate %float %float_1
               OpBranch %10037
      %10037 = OpLabel
      %11195 = OpPhi %v2float %12938 %24046 %11653 %8266
      %14248 = OpPhi %v4float %14676 %24046 %21595 %8266
               OpLoopMerge %14336 %8266 None
               OpBranch %14372
      %14372 = OpLabel
      %24294 = OpCompositeExtract %float %14248 2
      %11703 = OpFOrdLessThan %bool %24294 %float_15
               OpSelectionMerge %15748 None
               OpBranchConditional %11703 %11740 %15748
      %11740 = OpLabel
      %21564 = OpCompositeExtract %float %14248 3
      %14004 = OpFOrdGreaterThan %bool %21564 %float_0_9
               OpBranch %15748
      %15748 = OpLabel
      %10370 = OpPhi %bool %11703 %14372 %14004 %11740
               OpBranchConditional %10370 %8266 %14336
       %8266 = OpLabel
      %24744 = OpCompositeConstruct %v3float %float_1 %15185 %float_1
      %18284 = OpVectorShuffle %v3float %14248 %14248 0 1 2
      %16028 = OpExtInst %v3float %1 Fma %9034 %24744 %18284
      %20463 = OpVectorShuffle %v4float %14248 %16028 4 5 6 3
       %9474 = OpVectorShuffle %v2float %20463 %20463 0 1
       %6754 = OpImageSampleExplicitLod %v4float %7326 %9474 Lod %float_0
       %8765 = OpVectorShuffle %v2float %6754 %6754 0 1
      %21601 = OpCompositeExtract %float %6754 0
      %21302 = OpFMul %float %float_5 %21601
      %22336 = OpFSub %float %21302 %float_3_75
      %20695 = OpExtInst %float %1 FAbs %22336
      %21599 = OpFMul %float %21601 %20695
      %22878 = OpCompositeInsert %v2float %21599 %8765 0
      %11653 = OpExtInst %v2float %1 Round %22878
      %13943 = OpDot %float %11653 %1566
      %21595 = OpCompositeInsert %v4float %13943 %20463 3
               OpBranch %10037
      %14336 = OpLabel
      %17548 = OpVectorShuffle %v2float %14248 %14248 2 3
      %16543 = OpVectorShuffle %v4float %13336 %17548 0 4 2 5
       %9125 = OpCompositeExtract %float %11195 1
      %20439 = OpFOrdGreaterThan %bool %9125 %float_0_9
      %21884 = OpSelect %float %20439 %float_1 %float_0
      %17776 = OpCompositeExtract %float %14248 0
      %24309 = OpFAdd %float %17776 %21884
      %24095 = OpCompositeInsert %v4float %24309 %16543 1
               OpBranch %13108
      %12525 = OpLabel
      %10381 = OpVectorShuffle %v4float %13336 %1823 0 4 2 5
               OpBranch %13108
      %13108 = OpLabel
      %17360 = OpPhi %v4float %24095 %14336 %10381 %12525
      %23966 = OpCompositeExtract %float %17360 0
      %23848 = OpCompositeExtract %float %17360 1
      %12491 = OpFAdd %float %23966 %23848
      %24145 = OpFOrdGreaterThan %bool %12491 %float_2
               OpSelectionMerge %21110 None
               OpBranchConditional %24145 %21728 %21110
      %21728 = OpLabel
      %14921 = OpFNegate %float %23966
      %10954 = OpFNegate %float %23848
      %21235 = OpCompositeConstruct %v4float %14921 %23966 %23848 %10954
       %9126 = OpVectorShuffle %v4float %6934 %6934 0 1 0 1
      %18336 = OpVectorShuffle %v4float %11483 %11483 0 1 0 1
      %12162 = OpExtInst %v4float %1 Fma %21235 %9126 %18336
      %18770 = OpVectorShuffle %v2float %12162 %12162 0 1
      %13286 = OpImageSampleExplicitLod %v4float %7326 %18770 Lod|ConstOffset %float_0 %1806
      %10421 = OpCompositeExtract %float %13286 1
      %13287 = OpCompositeInsert %v4float %10421 %16660 0
      %19906 = OpImageSampleExplicitLod %v4float %7326 %18770 Lod|ConstOffset %float_0 %1827
      %15898 = OpCompositeExtract %float %19906 0
      %14977 = OpCompositeInsert %v4float %15898 %13287 1
      %16091 = OpVectorShuffle %v2float %12162 %12162 2 3
      %24977 = OpImageSampleExplicitLod %v4float %7326 %16091 Lod|ConstOffset %float_0 %1824
       %8086 = OpVectorShuffle %v2float %24977 %24977 1 0
       %8198 = OpVectorShuffle %v4float %14977 %8086 0 1 4 5
      %22016 = OpVectorShuffle %v2float %8198 %8198 0 2
      %14301 = OpVectorShuffle %v2float %8198 %8198 1 3
      %12010 = OpExtInst %v2float %1 Fma %2981 %22016 %14301
      %18892 = OpVectorShuffle %v2float %17360 %17360 2 3
      %12614 = OpExtInst %v2float %1 Step %25132 %18892
      %24679 = OpFOrdNotEqual %v2bool %12614 %1823
      %22789 = OpCompositeExtract %bool %24679 0
               OpSelectionMerge %18757 None
               OpBranchConditional %22789 %12762 %18757
      %12762 = OpLabel
      %21524 = OpCompositeInsert %v2float %float_0 %12010 0
               OpBranch %18757
      %18757 = OpLabel
      %20515 = OpPhi %v2float %12010 %21728 %21524 %12762
      %16619 = OpCompositeExtract %bool %24679 1
               OpSelectionMerge %18719 None
               OpBranchConditional %16619 %12763 %18719
      %12763 = OpLabel
      %21525 = OpCompositeInsert %v2float %float_0 %20515 1
               OpBranch %18719
      %18719 = OpLabel
      %16446 = OpPhi %v2float %20515 %18757 %21525 %12763
      %10516 = OpVectorShuffle %v2float %17360 %17360 0 1
       %8129 = OpExtInst %v2float %1 Fma %1893 %16446 %10516
       %9645 = OpExtInst %v2float %1 Fma %674 %8129 %1472
      %21242 = OpCompositeExtract %float %9645 0
      %14493 = OpFAdd %float %21242 %float_0_5
      %21919 = OpCompositeInsert %v2float %14493 %9645 0
       %9265 = OpCompositeExtract %float %9645 1
      %10325 = OpFSub %float %float_1 %9265
      %21366 = OpCompositeInsert %v2float %10325 %21919 1
       %6424 = OpLoad %510 %4961
      %17145 = OpImageSampleExplicitLod %v4float %6424 %21366 Lod %float_0
       %7431 = OpVectorShuffle %v2float %17145 %17145 0 1
      %21849 = OpVectorShuffle %v2float %7431 %7431 1 0
       %6952 = OpFAdd %v2float %19748 %21849
               OpBranch %21110
      %21110 = OpLabel
      %17706 = OpPhi %v2float %19748 %13108 %6952 %18719
      %20369 = OpVectorShuffle %v4float %2938 %17706 4 5 2 3
      %12458 = OpCompositeExtract %float %17706 0
      %19041 = OpCompositeExtract %float %17706 1
      %10843 = OpFNegate %float %19041
      %23044 = OpFOrdEqual %bool %12458 %10843
               OpSelectionMerge %21872 None
               OpBranchConditional %23044 %10087 %6357
      %10087 = OpLabel
      %17970 = OpVectorShuffle %v2float %20754 %20754 0 1
      %23026 = OpCompositeExtract %float %11482 0
               OpBranch %17837
      %17837 = OpLabel
      %11196 = OpPhi %v2float %17970 %10087 %8892 %6879
      %14249 = OpPhi %v2float %889 %10087 %13062 %6879
               OpLoopMerge %15186 %6879 None
               OpBranch %14407
      %14407 = OpLabel
      %23945 = OpCompositeExtract %float %11196 0
      %14386 = OpFOrdGreaterThan %bool %23945 %23026
               OpSelectionMerge %15688 None
               OpBranchConditional %14386 %11741 %15688
      %11741 = OpLabel
      %21565 = OpCompositeExtract %float %14249 1
      %14005 = OpFOrdGreaterThan %bool %21565 %float_0_8281
               OpBranch %15688
      %15688 = OpLabel
      %10924 = OpPhi %bool %14386 %14407 %14005 %11741
               OpSelectionMerge %17363 None
               OpBranchConditional %10924 %11622 %17363
      %11622 = OpLabel
      %22599 = OpCompositeExtract %float %14249 0
      %25017 = OpFOrdEqual %bool %22599 %float_0
               OpBranch %17363
      %17363 = OpLabel
      %10371 = OpPhi %bool %10924 %15688 %25017 %11622
               OpBranchConditional %10371 %6879 %15186
       %6879 = OpLabel
      %16410 = OpImageSampleExplicitLod %v4float %7326 %11196 Lod %float_0
      %13062 = OpVectorShuffle %v2float %16410 %16410 0 1
       %8892 = OpExtInst %v2float %1 Fma %2286 %16723 %11196
               OpBranch %17837
      %15186 = OpLabel
       %9252 = OpExtInst %v2float %1 Fma %25127 %14249 %25130
       %6230 = OpCompositeExtract %float %9252 1
      %15809 = OpFSub %float %float_1 %6230
      %14938 = OpCompositeInsert %v2float %15809 %9252 1
       %6461 = OpLoad %510 %5463
      %15576 = OpImageSampleExplicitLod %v4float %6461 %14938 Lod %float_0
      %18075 = OpCompositeExtract %float %15576 0
       %6640 = OpExtInst %float %1 Fma %float_n2_00787 %18075 %float_3_25
      %10172 = OpExtInst %float %1 Fma %25035 %6640 %23945
      %21352 = OpCompositeInsert %v3float %10172 %24831 0
      %25118 = OpCompositeExtract %float %23772 1
      %15905 = OpCompositeInsert %v3float %25118 %21352 1
      %12311 = OpCompositeInsert %v2float %10172 %12938 0
      %20164 = OpVectorShuffle %v2float %15905 %15905 0 1
      %24631 = OpImageSampleExplicitLod %v4float %7326 %20164 Lod %float_0
      %10807 = OpCompositeExtract %float %24631 0
      %10584 = OpVectorShuffle %v2float %20754 %20754 2 3
      %20409 = OpCompositeExtract %float %11482 1
               OpBranch %17838
      %17838 = OpLabel
      %11197 = OpPhi %v2float %10584 %15186 %8893 %6880
      %14250 = OpPhi %v2float %889 %15186 %13063 %6880
               OpLoopMerge %15187 %6880 None
               OpBranch %14373
      %14373 = OpLabel
      %24295 = OpCompositeExtract %float %11197 0
      %11704 = OpFOrdLessThan %bool %24295 %20409
               OpSelectionMerge %15689 None
               OpBranchConditional %11704 %11742 %15689
      %11742 = OpLabel
      %21566 = OpCompositeExtract %float %14250 1
      %14006 = OpFOrdGreaterThan %bool %21566 %float_0_8281
               OpBranch %15689
      %15689 = OpLabel
      %10925 = OpPhi %bool %11704 %14373 %14006 %11742
               OpSelectionMerge %17364 None
               OpBranchConditional %10925 %11623 %17364
      %11623 = OpLabel
      %22600 = OpCompositeExtract %float %14250 0
      %25018 = OpFOrdEqual %bool %22600 %float_0
               OpBranch %17364
      %17364 = OpLabel
      %10372 = OpPhi %bool %10925 %15689 %25018 %11623
               OpBranchConditional %10372 %6880 %15187
       %6880 = OpLabel
      %16411 = OpImageSampleExplicitLod %v4float %7326 %11197 Lod %float_0
      %13063 = OpVectorShuffle %v2float %16411 %16411 0 1
       %8893 = OpExtInst %v2float %1 Fma %2982 %16723 %11197
               OpBranch %17838
      %15187 = OpLabel
      %25097 = OpExtInst %v2float %1 Fma %25127 %14250 %25138
       %6231 = OpCompositeExtract %float %25097 1
      %16322 = OpFSub %float %float_1 %6231
      %10134 = OpCompositeInsert %v2float %16322 %25097 1
       %7595 = OpImageSampleExplicitLod %v4float %6461 %10134 Lod %float_0
       %8390 = OpCompositeExtract %float %7595 0
      %25066 = OpExtInst %float %1 Fma %float_n2_00787 %8390 %float_3_25
      %20247 = OpFNegate %float %25035
      %11693 = OpExtInst %float %1 Fma %20247 %25066 %24295
      %20689 = OpCompositeInsert %v3float %11693 %15905 2
      %17387 = OpCompositeInsert %v2float %11693 %12311 1
      %22595 = OpVectorShuffle %v2float %6934 %6934 2 2
      %19360 = OpVectorShuffle %v2float %11976 %11976 0 0
      %20255 = OpFNegate %v2float %19360
      %14035 = OpExtInst %v2float %1 Fma %22595 %17387 %20255
      %20211 = OpExtInst %v2float %1 Round %14035
      %12943 = OpExtInst %v2float %1 FAbs %20211
      %12627 = OpExtInst %v2float %1 Sqrt %12943
      %14877 = OpVectorShuffle %v2float %20689 %20689 2 1
       %9213 = OpImageSampleExplicitLod %v4float %7326 %14877 Lod|ConstOffset %float_0 %1824
      %11793 = OpCompositeExtract %float %9213 0
      %19021 = OpCompositeConstruct %v2float %10807 %11793
      %19233 = OpVectorTimesScalar %v2float %19021 %float_4
      %15227 = OpExtInst %v2float %1 Round %19233
      %19444 = OpExtInst %v2float %1 Fma %587 %15227 %12627
      %18072 = OpExtInst %v2float %1 Fma %674 %19444 %1472
      %21885 = OpCompositeExtract %float %18072 1
      %22027 = OpExtInst %float %1 Fma %float_0_142857 %float_0 %21885
      %14567 = OpFSub %float %float_1 %22027
      %13728 = OpCompositeInsert %v2float %14567 %18072 1
       %6425 = OpLoad %510 %4961
      %17146 = OpImageSampleExplicitLod %v4float %6425 %13728 Lod %float_0
       %6538 = OpVectorShuffle %v2float %17146 %17146 0 1
       %7679 = OpVectorShuffle %v4float %20369 %6538 4 5 2 3
      %17731 = OpCompositeInsert %v3float %12344 %20689 1
      %22492 = OpVectorShuffle %v2float %7679 %7679 0 1
       %8591 = OpVectorShuffle %v4float %17731 %17731 0 1 2 1
      %12504 = OpVectorShuffle %v2float %12943 %12943 1 0
      %19782 = OpExtInst %v2float %1 Step %12943 %12504
       %8388 = OpVectorTimesScalar %v2float %19782 %float_0_75
      %21925 = OpCompositeExtract %float %19782 0
       %6563 = OpCompositeExtract %float %19782 1
       %9648 = OpFAdd %float %21925 %6563
      %18714 = OpCompositeConstruct %v2float %9648 %9648
      %21387 = OpFDiv %v2float %8388 %18714
       %8280 = OpCompositeExtract %float %21387 0
      %12608 = OpVectorShuffle %v2float %8591 %8591 0 1
       %9008 = OpImageSampleExplicitLod %v4float %7326 %12608 Lod|ConstOffset %float_0 %1803
      %21486 = OpCompositeExtract %float %9008 0
       %7567 = OpFMul %float %8280 %21486
      %22457 = OpFSub %float %float_1 %7567
      %23213 = OpCompositeExtract %float %21387 1
      %11804 = OpVectorShuffle %v2float %8591 %8591 2 3
       %9009 = OpImageSampleExplicitLod %v4float %7326 %11804 Lod|ConstOffset %float_0 %1812
      %21487 = OpCompositeExtract %float %9009 0
       %7586 = OpFMul %float %23213 %21487
      %22455 = OpFSub %float %22457 %7586
      %13330 = OpCompositeInsert %v2float %22455 %768 0
       %9844 = OpImageSampleExplicitLod %v4float %7326 %12608 Lod|ConstOffset %float_0 %1839
       %8063 = OpCompositeExtract %float %9844 0
       %7700 = OpFMul %float %8280 %8063
      %21284 = OpFSub %float %float_1 %7700
      %23584 = OpImageSampleExplicitLod %v4float %7326 %11804 Lod|ConstOffset %float_0 %1848
      %12068 = OpCompositeExtract %float %23584 0
       %7587 = OpFMul %float %23213 %12068
      %22303 = OpFSub %float %21284 %7587
      %14695 = OpCompositeInsert %v2float %22303 %13330 1
      %17435 = OpExtInst %v2float %1 FClamp %14695 %1823 %768
      %22452 = OpFMul %v2float %22492 %17435
      %21602 = OpVectorShuffle %v4float %7679 %22452 4 5 2 3
               OpBranch %21872
       %6357 = OpLabel
      %23943 = OpCompositeInsert %v2float %float_0 %8417 0
               OpBranch %21872
      %21872 = OpLabel
      %11251 = OpPhi %v4float %21602 %15187 %20369 %6357
      %13709 = OpPhi %v2float %8417 %15187 %23943 %6357
               OpBranch %21263
      %21263 = OpLabel
       %8059 = OpPhi %v4float %2938 %24607 %11251 %21872
       %9910 = OpPhi %v2float %8417 %24607 %13709 %21872
       %8852 = OpCompositeExtract %float %9910 0
      %15194 = OpFOrdGreaterThan %bool %8852 %float_0
               OpSelectionMerge %12747 None
               OpBranchConditional %15194 %10088 %12747
      %10088 = OpLabel
      %17971 = OpVectorShuffle %v2float %23772 %23772 0 1
      %23027 = OpCompositeExtract %float %11482 2
               OpBranch %17839
      %17839 = OpLabel
      %11198 = OpPhi %v2float %17971 %10088 %22330 %6881
      %14251 = OpPhi %v2float %312 %10088 %20851 %6881
               OpLoopMerge %14352 %6881 None
               OpBranch %14374
      %14374 = OpLabel
      %24296 = OpCompositeExtract %float %11198 1
      %11705 = OpFOrdLessThan %bool %24296 %23027
               OpSelectionMerge %15690 None
               OpBranchConditional %11705 %11743 %15690
      %11743 = OpLabel
      %21567 = OpCompositeExtract %float %14251 0
      %14007 = OpFOrdGreaterThan %bool %21567 %float_0_8281
               OpBranch %15690
      %15690 = OpLabel
      %10926 = OpPhi %bool %11705 %14374 %14007 %11743
               OpSelectionMerge %17365 None
               OpBranchConditional %10926 %11624 %17365
      %11624 = OpLabel
      %22601 = OpCompositeExtract %float %14251 1
      %25019 = OpFOrdEqual %bool %22601 %float_0
               OpBranch %17365
      %17365 = OpLabel
      %10373 = OpPhi %bool %10926 %15690 %25019 %11624
               OpBranchConditional %10373 %6881 %14352
       %6881 = OpLabel
      %17367 = OpImageSampleExplicitLod %v4float %7326 %11198 Lod %float_0
      %20851 = OpVectorShuffle %v2float %17367 %17367 0 1
      %11588 = OpAccessChain %_ptr_Uniform_v4float %4930 %int_0
       %8094 = OpLoad %v4float %11588
      %11321 = OpVectorShuffle %v2float %8094 %8094 0 1
      %22330 = OpExtInst %v2float %1 Fma %1128 %11321 %11198
               OpBranch %17839
      %14352 = OpLabel
      %18361 = OpVectorShuffle %v2float %14251 %14251 1 0
       %9253 = OpExtInst %v2float %1 Fma %25127 %18361 %25130
       %6232 = OpCompositeExtract %float %9253 1
      %15810 = OpFSub %float %float_1 %6232
      %14939 = OpCompositeInsert %v2float %15810 %9253 1
       %6462 = OpLoad %510 %5463
      %15577 = OpImageSampleExplicitLod %v4float %6462 %14939 Lod %float_0
      %19032 = OpCompositeExtract %float %15577 0
      %17602 = OpExtInst %float %1 Fma %float_n2_00787 %19032 %float_3_25
       %7133 = OpAccessChain %_ptr_Uniform_float %4930 %int_0 %uint_1
      %19264 = OpLoad %float %7133
       %8918 = OpFNegate %float %17602
       %9668 = OpExtInst %float %1 Fma %19264 %8918 %24296
      %20919 = OpCompositeInsert %v3float %9668 %24831 1
      %25119 = OpCompositeExtract %float %20754 0
      %15906 = OpCompositeInsert %v3float %25119 %20919 0
      %12312 = OpCompositeInsert %v2float %9668 %12938 0
      %20165 = OpVectorShuffle %v2float %15906 %15906 0 1
      %24632 = OpImageSampleExplicitLod %v4float %7326 %20165 Lod %float_0
      %10808 = OpCompositeExtract %float %24632 1
      %10585 = OpVectorShuffle %v2float %23772 %23772 2 3
      %20410 = OpCompositeExtract %float %11482 3
               OpBranch %17840
      %17840 = OpLabel
      %11199 = OpPhi %v2float %10585 %14352 %22331 %6882
      %14252 = OpPhi %v2float %312 %14352 %20852 %6882
               OpLoopMerge %14353 %6882 None
               OpBranch %14408
      %14408 = OpLabel
      %23946 = OpCompositeExtract %float %11199 1
      %14387 = OpFOrdGreaterThan %bool %23946 %20410
               OpSelectionMerge %15691 None
               OpBranchConditional %14387 %11744 %15691
      %11744 = OpLabel
      %21568 = OpCompositeExtract %float %14252 0
      %14008 = OpFOrdGreaterThan %bool %21568 %float_0_8281
               OpBranch %15691
      %15691 = OpLabel
      %10927 = OpPhi %bool %14387 %14408 %14008 %11744
               OpSelectionMerge %17366 None
               OpBranchConditional %10927 %11625 %17366
      %11625 = OpLabel
      %22602 = OpCompositeExtract %float %14252 1
      %25020 = OpFOrdEqual %bool %22602 %float_0
               OpBranch %17366
      %17366 = OpLabel
      %10374 = OpPhi %bool %10927 %15691 %25020 %11625
               OpBranchConditional %10374 %6882 %14353
       %6882 = OpLabel
      %17368 = OpImageSampleExplicitLod %v4float %7326 %11199 Lod %float_0
      %20852 = OpVectorShuffle %v2float %17368 %17368 0 1
      %11589 = OpAccessChain %_ptr_Uniform_v4float %4930 %int_0
       %8095 = OpLoad %v4float %11589
      %11322 = OpVectorShuffle %v2float %8095 %8095 0 1
      %22331 = OpExtInst %v2float %1 Fma %1825 %11322 %11199
               OpBranch %17840
      %14353 = OpLabel
      %18362 = OpVectorShuffle %v2float %14252 %14252 1 0
      %25098 = OpExtInst %v2float %1 Fma %25127 %18362 %25138
       %6233 = OpCompositeExtract %float %25098 1
      %16323 = OpFSub %float %float_1 %6233
      %10135 = OpCompositeInsert %v2float %16323 %25098 1
       %7596 = OpImageSampleExplicitLod %v4float %6462 %10135 Lod %float_0
       %8391 = OpCompositeExtract %float %7596 0
       %8130 = OpExtInst %float %1 Fma %float_n2_00787 %8391 %float_3_25
      %19602 = OpFNegate %float %19264
      %21749 = OpFNegate %float %8130
      %17717 = OpExtInst %float %1 Fma %19602 %21749 %23946
      %20423 = OpCompositeInsert %v3float %17717 %15906 2
      %19467 = OpCompositeInsert %v2float %17717 %12312 1
      %25073 = OpAccessChain %_ptr_Uniform_v4float %4930 %int_0
       %8508 = OpLoad %v4float %25073
      %17942 = OpVectorShuffle %v2float %8508 %8508 3 3
      %12109 = OpVectorShuffle %v2float %11976 %11976 1 1
      %20256 = OpFNegate %v2float %12109
      %14036 = OpExtInst %v2float %1 Fma %17942 %19467 %20256
      %20212 = OpExtInst %v2float %1 Round %14036
      %12944 = OpExtInst %v2float %1 FAbs %20212
      %12628 = OpExtInst %v2float %1 Sqrt %12944
      %14878 = OpVectorShuffle %v2float %20423 %20423 0 2
       %9214 = OpImageSampleExplicitLod %v4float %7326 %14878 Lod|ConstOffset %float_0 %1803
      %11794 = OpCompositeExtract %float %9214 1
      %19022 = OpCompositeConstruct %v2float %10808 %11794
      %19234 = OpVectorTimesScalar %v2float %19022 %float_4
      %15228 = OpExtInst %v2float %1 Round %19234
      %19445 = OpExtInst %v2float %1 Fma %587 %15228 %12628
      %18073 = OpExtInst %v2float %1 Fma %674 %19445 %1472
      %21886 = OpCompositeExtract %float %18073 1
      %22028 = OpExtInst %float %1 Fma %float_0_142857 %float_0 %21886
      %14568 = OpFSub %float %float_1 %22028
      %13729 = OpCompositeInsert %v2float %14568 %18073 1
       %6426 = OpLoad %510 %4961
      %17147 = OpImageSampleExplicitLod %v4float %6426 %13729 Lod %float_0
       %6519 = OpVectorShuffle %v2float %17147 %17147 0 1
       %7909 = OpVectorShuffle %v4float %8059 %6519 0 1 4 5
       %6334 = OpCompositeExtract %float %11483 0
      %17233 = OpCompositeInsert %v3float %6334 %20423 0
      %24105 = OpVectorShuffle %v2float %7909 %7909 2 3
       %8592 = OpVectorShuffle %v4float %17233 %17233 0 1 0 2
      %12505 = OpVectorShuffle %v2float %12944 %12944 1 0
      %19783 = OpExtInst %v2float %1 Step %12944 %12505
       %8389 = OpVectorTimesScalar %v2float %19783 %float_0_75
      %21926 = OpCompositeExtract %float %19783 0
       %6564 = OpCompositeExtract %float %19783 1
       %9649 = OpFAdd %float %21926 %6564
      %18715 = OpCompositeConstruct %v2float %9649 %9649
      %21388 = OpFDiv %v2float %8389 %18715
       %8281 = OpCompositeExtract %float %21388 0
      %12609 = OpVectorShuffle %v2float %8592 %8592 0 1
       %9010 = OpImageSampleExplicitLod %v4float %7326 %12609 Lod|ConstOffset %float_0 %1824
      %21488 = OpCompositeExtract %float %9010 1
       %7568 = OpFMul %float %8281 %21488
      %22458 = OpFSub %float %float_1 %7568
      %23214 = OpCompositeExtract %float %21388 1
      %11805 = OpVectorShuffle %v2float %8592 %8592 2 3
       %9011 = OpImageSampleExplicitLod %v4float %7326 %11805 Lod|ConstOffset %float_0 %1812
      %21489 = OpCompositeExtract %float %9011 1
       %7588 = OpFMul %float %23214 %21489
      %22459 = OpFSub %float %22458 %7588
      %13331 = OpCompositeInsert %v2float %22459 %768 0
       %9845 = OpImageSampleExplicitLod %v4float %7326 %12609 Lod|ConstOffset %float_0 %1797
       %8064 = OpCompositeExtract %float %9845 1
       %7701 = OpFMul %float %8281 %8064
      %21285 = OpFSub %float %float_1 %7701
      %23585 = OpImageSampleExplicitLod %v4float %7326 %11805 Lod|ConstOffset %float_0 %1785
      %12069 = OpCompositeExtract %float %23585 1
       %7589 = OpFMul %float %23214 %12069
      %22304 = OpFSub %float %21285 %7589
      %14696 = OpCompositeInsert %v2float %22304 %13331 1
      %17436 = OpExtInst %v2float %1 FClamp %14696 %1823 %768
      %22460 = OpFMul %v2float %24105 %17436
      %21603 = OpVectorShuffle %v4float %7909 %22460 0 1 4 5
               OpBranch %12747
      %12747 = OpLabel
      %23915 = OpPhi %v4float %8059 %21263 %21603 %14353
               OpStore %3569 %23915
               OpReturn
               OpFunctionEnd
