// GENERATED FILE - DO NOT EDIT.
// Generated by generate_tests.py
//
// Copyright (c) 2022 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../diff_test_utils.h"

#include "gtest/gtest.h"

namespace spvtools {
namespace diff {
namespace {

// Test where src and dst have the true and false blocks of an if reordered.
constexpr char kSrc[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %8 %44
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
               OpName %4 "main"
               OpName %8 "v"
               OpName %44 "color"
               OpDecorate %8 RelaxedPrecision
               OpDecorate %8 Location 0
               OpDecorate %9 RelaxedPrecision
               OpDecorate %18 RelaxedPrecision
               OpDecorate %19 RelaxedPrecision
               OpDecorate %20 RelaxedPrecision
               OpDecorate %23 RelaxedPrecision
               OpDecorate %24 RelaxedPrecision
               OpDecorate %25 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
               OpDecorate %28 RelaxedPrecision
               OpDecorate %29 RelaxedPrecision
               OpDecorate %30 RelaxedPrecision
               OpDecorate %31 RelaxedPrecision
               OpDecorate %33 RelaxedPrecision
               OpDecorate %34 RelaxedPrecision
               OpDecorate %35 RelaxedPrecision
               OpDecorate %36 RelaxedPrecision
               OpDecorate %37 RelaxedPrecision
               OpDecorate %39 RelaxedPrecision
               OpDecorate %40 RelaxedPrecision
               OpDecorate %41 RelaxedPrecision
               OpDecorate %42 RelaxedPrecision
               OpDecorate %44 RelaxedPrecision
               OpDecorate %44 Location 0
               OpDecorate %45 RelaxedPrecision
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeFloat 32
          %7 = OpTypePointer Input %6
          %8 = OpVariable %7 Input
         %10 = OpConstant %6 0
         %11 = OpTypeBool
         %15 = OpTypeVector %6 4
         %16 = OpTypePointer Function %15
         %21 = OpConstant %6 -0.5
         %22 = OpConstant %6 -0.300000012
         %38 = OpConstant %6 0.5
         %43 = OpTypePointer Output %15
         %44 = OpVariable %43 Output
          %4 = OpFunction %2 None %3
          %5 = OpLabel
          %9 = OpLoad %6 %8
         %12 = OpFOrdLessThanEqual %11 %9 %10
               OpSelectionMerge %14 None
               OpBranchConditional %12 %13 %32
         %13 = OpLabel
         %18 = OpLoad %6 %8
         %19 = OpExtInst %6 %1 Log %18
         %20 = OpLoad %6 %8
         %23 = OpExtInst %6 %1 FClamp %20 %21 %22
         %24 = OpFMul %6 %19 %23
         %25 = OpLoad %6 %8
         %26 = OpExtInst %6 %1 Sin %25
         %27 = OpLoad %6 %8
         %28 = OpExtInst %6 %1 Cos %27
         %29 = OpLoad %6 %8
         %30 = OpExtInst %6 %1 Exp %29
         %31 = OpCompositeConstruct %15 %24 %26 %28 %30
               OpBranch %14
         %32 = OpLabel
         %33 = OpLoad %6 %8
         %34 = OpExtInst %6 %1 Sqrt %33
         %35 = OpLoad %6 %8
         %36 = OpExtInst %6 %1 FSign %35
         %37 = OpLoad %6 %8
         %39 = OpExtInst %6 %1 FMax %37 %38
         %40 = OpLoad %6 %8
         %41 = OpExtInst %6 %1 Floor %40
         %42 = OpCompositeConstruct %15 %34 %36 %39 %41
               OpBranch %14
         %14 = OpLabel
         %45 = OpPhi %15 %31 %13 %42 %32
               OpStore %44 %45
               OpReturn
               OpFunctionEnd)";
constexpr char kDst[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %8 %44
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
               OpName %4 "main"
               OpName %8 "v"
               OpName %44 "color"
               OpDecorate %8 RelaxedPrecision
               OpDecorate %8 Location 0
               OpDecorate %9 RelaxedPrecision
               OpDecorate %18 RelaxedPrecision
               OpDecorate %19 RelaxedPrecision
               OpDecorate %20 RelaxedPrecision
               OpDecorate %21 RelaxedPrecision
               OpDecorate %22 RelaxedPrecision
               OpDecorate %24 RelaxedPrecision
               OpDecorate %25 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
               OpDecorate %29 RelaxedPrecision
               OpDecorate %30 RelaxedPrecision
               OpDecorate %31 RelaxedPrecision
               OpDecorate %34 RelaxedPrecision
               OpDecorate %35 RelaxedPrecision
               OpDecorate %36 RelaxedPrecision
               OpDecorate %37 RelaxedPrecision
               OpDecorate %38 RelaxedPrecision
               OpDecorate %39 RelaxedPrecision
               OpDecorate %40 RelaxedPrecision
               OpDecorate %41 RelaxedPrecision
               OpDecorate %42 RelaxedPrecision
               OpDecorate %44 RelaxedPrecision
               OpDecorate %44 Location 0
               OpDecorate %45 RelaxedPrecision
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeFloat 32
          %7 = OpTypePointer Input %6
          %8 = OpVariable %7 Input
         %10 = OpConstant %6 0
         %11 = OpTypeBool
         %15 = OpTypeVector %6 4
         %16 = OpTypePointer Function %15
         %23 = OpConstant %6 0.5
         %32 = OpConstant %6 -0.5
         %33 = OpConstant %6 -0.300000012
         %43 = OpTypePointer Output %15
         %44 = OpVariable %43 Output
          %4 = OpFunction %2 None %3
          %5 = OpLabel
          %9 = OpLoad %6 %8
         %12 = OpFOrdLessThanEqual %11 %9 %10
               OpSelectionMerge %14 None
               OpBranchConditional %12 %28 %13
         %13 = OpLabel
         %18 = OpLoad %6 %8
         %19 = OpExtInst %6 %1 Sqrt %18
         %20 = OpLoad %6 %8
         %21 = OpExtInst %6 %1 FSign %20
         %22 = OpLoad %6 %8
         %24 = OpExtInst %6 %1 FMax %22 %23
         %25 = OpLoad %6 %8
         %26 = OpExtInst %6 %1 Floor %25
         %27 = OpCompositeConstruct %15 %19 %21 %24 %26
               OpBranch %14
         %28 = OpLabel
         %29 = OpLoad %6 %8
         %30 = OpExtInst %6 %1 Log %29
         %31 = OpLoad %6 %8
         %34 = OpExtInst %6 %1 FClamp %31 %32 %33
         %35 = OpFMul %6 %30 %34
         %36 = OpLoad %6 %8
         %37 = OpExtInst %6 %1 Sin %36
         %38 = OpLoad %6 %8
         %39 = OpExtInst %6 %1 Cos %38
         %40 = OpLoad %6 %8
         %41 = OpExtInst %6 %1 Exp %40
         %42 = OpCompositeConstruct %15 %35 %37 %39 %41
               OpBranch %14
         %14 = OpLabel
         %45 = OpPhi %15 %27 %13 %42 %28
               OpStore %44 %45
               OpReturn
               OpFunctionEnd

)";

TEST(DiffTest, ReorderedIfBlocks) {
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
-; Bound: 46
+; Bound: 47
 ; Schema: 0
 OpCapability Shader
 %1 = OpExtInstImport "GLSL.std.450"
 OpMemoryModel Logical GLSL450
 OpEntryPoint Fragment %4 "main" %8 %44
 OpExecutionMode %4 OriginUpperLeft
 OpSource ESSL 310
 OpName %4 "main"
 OpName %8 "v"
 OpName %44 "color"
 OpDecorate %8 RelaxedPrecision
 OpDecorate %8 Location 0
 OpDecorate %9 RelaxedPrecision
 OpDecorate %18 RelaxedPrecision
 OpDecorate %19 RelaxedPrecision
 OpDecorate %20 RelaxedPrecision
 OpDecorate %23 RelaxedPrecision
 OpDecorate %24 RelaxedPrecision
 OpDecorate %25 RelaxedPrecision
 OpDecorate %26 RelaxedPrecision
 OpDecorate %27 RelaxedPrecision
 OpDecorate %28 RelaxedPrecision
 OpDecorate %29 RelaxedPrecision
 OpDecorate %30 RelaxedPrecision
 OpDecorate %31 RelaxedPrecision
 OpDecorate %33 RelaxedPrecision
 OpDecorate %34 RelaxedPrecision
 OpDecorate %35 RelaxedPrecision
 OpDecorate %36 RelaxedPrecision
 OpDecorate %37 RelaxedPrecision
 OpDecorate %39 RelaxedPrecision
 OpDecorate %40 RelaxedPrecision
 OpDecorate %41 RelaxedPrecision
 OpDecorate %42 RelaxedPrecision
 OpDecorate %44 RelaxedPrecision
 OpDecorate %44 Location 0
 OpDecorate %45 RelaxedPrecision
 %2 = OpTypeVoid
 %3 = OpTypeFunction %2
 %6 = OpTypeFloat 32
 %7 = OpTypePointer Input %6
 %8 = OpVariable %7 Input
 %10 = OpConstant %6 0
 %11 = OpTypeBool
 %15 = OpTypeVector %6 4
 %16 = OpTypePointer Function %15
 %21 = OpConstant %6 -0.5
 %22 = OpConstant %6 -0.300000012
 %38 = OpConstant %6 0.5
 %43 = OpTypePointer Output %15
 %44 = OpVariable %43 Output
 %4 = OpFunction %2 None %3
 %5 = OpLabel
 %9 = OpLoad %6 %8
 %12 = OpFOrdLessThanEqual %11 %9 %10
 OpSelectionMerge %14 None
 OpBranchConditional %12 %13 %32
 %32 = OpLabel
 %33 = OpLoad %6 %8
 %34 = OpExtInst %6 %1 Sqrt %33
 %35 = OpLoad %6 %8
 %36 = OpExtInst %6 %1 FSign %35
 %37 = OpLoad %6 %8
 %39 = OpExtInst %6 %1 FMax %37 %38
 %40 = OpLoad %6 %8
 %41 = OpExtInst %6 %1 Floor %40
 %42 = OpCompositeConstruct %15 %34 %36 %39 %41
 OpBranch %14
 %13 = OpLabel
 %18 = OpLoad %6 %8
 %19 = OpExtInst %6 %1 Log %18
 %20 = OpLoad %6 %8
 %23 = OpExtInst %6 %1 FClamp %20 %21 %22
 %24 = OpFMul %6 %19 %23
 %25 = OpLoad %6 %8
 %26 = OpExtInst %6 %1 Sin %25
 %27 = OpLoad %6 %8
 %28 = OpExtInst %6 %1 Cos %27
 %29 = OpLoad %6 %8
 %30 = OpExtInst %6 %1 Exp %29
 %31 = OpCompositeConstruct %15 %24 %26 %28 %30
 OpBranch %14
 %14 = OpLabel
-%45 = OpPhi %15 %31 %13 %42 %32
+%45 = OpPhi %15 %42 %32 %31 %13
 OpStore %44 %45
 OpReturn
 OpFunctionEnd
)";
  Options options;
  DoStringDiffTest(kSrc, kDst, kDiff, options);
}

TEST(DiffTest, ReorderedIfBlocksNoDebug) {
  constexpr char kSrcNoDebug[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %8 %44
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
               OpDecorate %8 RelaxedPrecision
               OpDecorate %8 Location 0
               OpDecorate %9 RelaxedPrecision
               OpDecorate %18 RelaxedPrecision
               OpDecorate %19 RelaxedPrecision
               OpDecorate %20 RelaxedPrecision
               OpDecorate %23 RelaxedPrecision
               OpDecorate %24 RelaxedPrecision
               OpDecorate %25 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
               OpDecorate %28 RelaxedPrecision
               OpDecorate %29 RelaxedPrecision
               OpDecorate %30 RelaxedPrecision
               OpDecorate %31 RelaxedPrecision
               OpDecorate %33 RelaxedPrecision
               OpDecorate %34 RelaxedPrecision
               OpDecorate %35 RelaxedPrecision
               OpDecorate %36 RelaxedPrecision
               OpDecorate %37 RelaxedPrecision
               OpDecorate %39 RelaxedPrecision
               OpDecorate %40 RelaxedPrecision
               OpDecorate %41 RelaxedPrecision
               OpDecorate %42 RelaxedPrecision
               OpDecorate %44 RelaxedPrecision
               OpDecorate %44 Location 0
               OpDecorate %45 RelaxedPrecision
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeFloat 32
          %7 = OpTypePointer Input %6
          %8 = OpVariable %7 Input
         %10 = OpConstant %6 0
         %11 = OpTypeBool
         %15 = OpTypeVector %6 4
         %16 = OpTypePointer Function %15
         %21 = OpConstant %6 -0.5
         %22 = OpConstant %6 -0.300000012
         %38 = OpConstant %6 0.5
         %43 = OpTypePointer Output %15
         %44 = OpVariable %43 Output
          %4 = OpFunction %2 None %3
          %5 = OpLabel
          %9 = OpLoad %6 %8
         %12 = OpFOrdLessThanEqual %11 %9 %10
               OpSelectionMerge %14 None
               OpBranchConditional %12 %13 %32
         %13 = OpLabel
         %18 = OpLoad %6 %8
         %19 = OpExtInst %6 %1 Log %18
         %20 = OpLoad %6 %8
         %23 = OpExtInst %6 %1 FClamp %20 %21 %22
         %24 = OpFMul %6 %19 %23
         %25 = OpLoad %6 %8
         %26 = OpExtInst %6 %1 Sin %25
         %27 = OpLoad %6 %8
         %28 = OpExtInst %6 %1 Cos %27
         %29 = OpLoad %6 %8
         %30 = OpExtInst %6 %1 Exp %29
         %31 = OpCompositeConstruct %15 %24 %26 %28 %30
               OpBranch %14
         %32 = OpLabel
         %33 = OpLoad %6 %8
         %34 = OpExtInst %6 %1 Sqrt %33
         %35 = OpLoad %6 %8
         %36 = OpExtInst %6 %1 FSign %35
         %37 = OpLoad %6 %8
         %39 = OpExtInst %6 %1 FMax %37 %38
         %40 = OpLoad %6 %8
         %41 = OpExtInst %6 %1 Floor %40
         %42 = OpCompositeConstruct %15 %34 %36 %39 %41
               OpBranch %14
         %14 = OpLabel
         %45 = OpPhi %15 %31 %13 %42 %32
               OpStore %44 %45
               OpReturn
               OpFunctionEnd
)";
  constexpr char kDstNoDebug[] = R"(               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %8 %44
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
               OpDecorate %8 RelaxedPrecision
               OpDecorate %8 Location 0
               OpDecorate %9 RelaxedPrecision
               OpDecorate %18 RelaxedPrecision
               OpDecorate %19 RelaxedPrecision
               OpDecorate %20 RelaxedPrecision
               OpDecorate %21 RelaxedPrecision
               OpDecorate %22 RelaxedPrecision
               OpDecorate %24 RelaxedPrecision
               OpDecorate %25 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
               OpDecorate %29 RelaxedPrecision
               OpDecorate %30 RelaxedPrecision
               OpDecorate %31 RelaxedPrecision
               OpDecorate %34 RelaxedPrecision
               OpDecorate %35 RelaxedPrecision
               OpDecorate %36 RelaxedPrecision
               OpDecorate %37 RelaxedPrecision
               OpDecorate %38 RelaxedPrecision
               OpDecorate %39 RelaxedPrecision
               OpDecorate %40 RelaxedPrecision
               OpDecorate %41 RelaxedPrecision
               OpDecorate %42 RelaxedPrecision
               OpDecorate %44 RelaxedPrecision
               OpDecorate %44 Location 0
               OpDecorate %45 RelaxedPrecision
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeFloat 32
          %7 = OpTypePointer Input %6
          %8 = OpVariable %7 Input
         %10 = OpConstant %6 0
         %11 = OpTypeBool
         %15 = OpTypeVector %6 4
         %16 = OpTypePointer Function %15
         %23 = OpConstant %6 0.5
         %32 = OpConstant %6 -0.5
         %33 = OpConstant %6 -0.300000012
         %43 = OpTypePointer Output %15
         %44 = OpVariable %43 Output
          %4 = OpFunction %2 None %3
          %5 = OpLabel
          %9 = OpLoad %6 %8
         %12 = OpFOrdLessThanEqual %11 %9 %10
               OpSelectionMerge %14 None
               OpBranchConditional %12 %28 %13
         %13 = OpLabel
         %18 = OpLoad %6 %8
         %19 = OpExtInst %6 %1 Sqrt %18
         %20 = OpLoad %6 %8
         %21 = OpExtInst %6 %1 FSign %20
         %22 = OpLoad %6 %8
         %24 = OpExtInst %6 %1 FMax %22 %23
         %25 = OpLoad %6 %8
         %26 = OpExtInst %6 %1 Floor %25
         %27 = OpCompositeConstruct %15 %19 %21 %24 %26
               OpBranch %14
         %28 = OpLabel
         %29 = OpLoad %6 %8
         %30 = OpExtInst %6 %1 Log %29
         %31 = OpLoad %6 %8
         %34 = OpExtInst %6 %1 FClamp %31 %32 %33
         %35 = OpFMul %6 %30 %34
         %36 = OpLoad %6 %8
         %37 = OpExtInst %6 %1 Sin %36
         %38 = OpLoad %6 %8
         %39 = OpExtInst %6 %1 Cos %38
         %40 = OpLoad %6 %8
         %41 = OpExtInst %6 %1 Exp %40
         %42 = OpCompositeConstruct %15 %35 %37 %39 %41
               OpBranch %14
         %14 = OpLabel
         %45 = OpPhi %15 %27 %13 %42 %28
               OpStore %44 %45
               OpReturn
               OpFunctionEnd

)";
  constexpr char kDiff[] = R"( ; SPIR-V
 ; Version: 1.6
 ; Generator: Khronos SPIR-V Tools Assembler; 0
-; Bound: 46
+; Bound: 47
 ; Schema: 0
 OpCapability Shader
 %1 = OpExtInstImport "GLSL.std.450"
 OpMemoryModel Logical GLSL450
 OpEntryPoint Fragment %4 "main" %8 %44
 OpExecutionMode %4 OriginUpperLeft
 OpSource ESSL 310
 OpDecorate %8 RelaxedPrecision
 OpDecorate %8 Location 0
 OpDecorate %9 RelaxedPrecision
 OpDecorate %18 RelaxedPrecision
 OpDecorate %19 RelaxedPrecision
 OpDecorate %20 RelaxedPrecision
 OpDecorate %23 RelaxedPrecision
 OpDecorate %24 RelaxedPrecision
 OpDecorate %25 RelaxedPrecision
 OpDecorate %26 RelaxedPrecision
 OpDecorate %27 RelaxedPrecision
 OpDecorate %28 RelaxedPrecision
 OpDecorate %29 RelaxedPrecision
 OpDecorate %30 RelaxedPrecision
 OpDecorate %31 RelaxedPrecision
 OpDecorate %33 RelaxedPrecision
 OpDecorate %34 RelaxedPrecision
 OpDecorate %35 RelaxedPrecision
 OpDecorate %36 RelaxedPrecision
 OpDecorate %37 RelaxedPrecision
 OpDecorate %39 RelaxedPrecision
 OpDecorate %40 RelaxedPrecision
 OpDecorate %41 RelaxedPrecision
 OpDecorate %42 RelaxedPrecision
 OpDecorate %44 RelaxedPrecision
 OpDecorate %44 Location 0
 OpDecorate %45 RelaxedPrecision
 %2 = OpTypeVoid
 %3 = OpTypeFunction %2
 %6 = OpTypeFloat 32
 %7 = OpTypePointer Input %6
 %8 = OpVariable %7 Input
 %10 = OpConstant %6 0
 %11 = OpTypeBool
 %15 = OpTypeVector %6 4
 %16 = OpTypePointer Function %15
 %21 = OpConstant %6 -0.5
 %22 = OpConstant %6 -0.300000012
 %38 = OpConstant %6 0.5
 %43 = OpTypePointer Output %15
 %44 = OpVariable %43 Output
 %4 = OpFunction %2 None %3
 %5 = OpLabel
 %9 = OpLoad %6 %8
 %12 = OpFOrdLessThanEqual %11 %9 %10
 OpSelectionMerge %14 None
 OpBranchConditional %12 %13 %32
 %32 = OpLabel
 %33 = OpLoad %6 %8
 %34 = OpExtInst %6 %1 Sqrt %33
 %35 = OpLoad %6 %8
 %36 = OpExtInst %6 %1 FSign %35
 %37 = OpLoad %6 %8
 %39 = OpExtInst %6 %1 FMax %37 %38
 %40 = OpLoad %6 %8
 %41 = OpExtInst %6 %1 Floor %40
 %42 = OpCompositeConstruct %15 %34 %36 %39 %41
 OpBranch %14
 %13 = OpLabel
 %18 = OpLoad %6 %8
 %19 = OpExtInst %6 %1 Log %18
 %20 = OpLoad %6 %8
 %23 = OpExtInst %6 %1 FClamp %20 %21 %22
 %24 = OpFMul %6 %19 %23
 %25 = OpLoad %6 %8
 %26 = OpExtInst %6 %1 Sin %25
 %27 = OpLoad %6 %8
 %28 = OpExtInst %6 %1 Cos %27
 %29 = OpLoad %6 %8
 %30 = OpExtInst %6 %1 Exp %29
 %31 = OpCompositeConstruct %15 %24 %26 %28 %30
 OpBranch %14
 %14 = OpLabel
-%45 = OpPhi %15 %31 %13 %42 %32
+%45 = OpPhi %15 %42 %32 %31 %13
 OpStore %44 %45
 OpReturn
 OpFunctionEnd
)";
  Options options;
  DoStringDiffTest(kSrcNoDebug, kDstNoDebug, kDiff, options);
}

}  // namespace
}  // namespace diff
}  // namespace spvtools