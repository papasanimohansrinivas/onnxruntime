// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/cpu/tensor/unsqueeze.h"
#include "core/providers/cpu/tensor/squeeze.h"
#include "utils.h"
#include "core/providers/common.h"

using namespace ::onnxruntime::common;

namespace onnxruntime {

ONNX_CPU_OPERATOR_VERSIONED_KERNEL(
    Unsqueeze,
    1,
    10,
    KernelDefBuilder()
        .Alias(0, 0)
        .TypeConstraint("T", DataTypeImpl::AllTensorTypes()),
    Unsqueeze);

ONNX_CPU_OPERATOR_VERSIONED_KERNEL(
    Unsqueeze,
    11,
    12,
    KernelDefBuilder()
        .Alias(0, 0)
        .TypeConstraint("T", DataTypeImpl::AllTensorTypes()),
    Unsqueeze);

// axes is input instead of attribute
ONNX_CPU_OPERATOR_KERNEL(
    Unsqueeze,
    13,
    KernelDefBuilder()
        .Alias(0, 0)
        .TypeConstraint("T", DataTypeImpl::AllTensorTypes()),
    Unsqueeze);

Status UnsqueezeBase::PrepareCompute(OpKernelContext* ctx, const TensorShape& input_shape, TensorShape& output_shape) const {

  std::vector<int64_t> axes = SqueezeBase::ComputeAxes(ctx, axes_);
#if !defined(DISABLE_SPARSE_TENSORS)
  if (ctx->InputType(0)->IsSparseTensorType()) {
    ORT_RETURN_IF_NOT(axes.size() <= 1, "Axes expected to have at most 1 element for sparse Unsqueeze");
    if (!axes.empty()) {
      ORT_RETURN_IF_NOT(axes[0] == 0 || axes[0] == 1, "Axes entry may be either 0 or 1 for sparse tensors");
    }
  }
#endif
  // New dimension count is the current dimensions + the number of entries in axes
  // Initialize output_dims to 0 in each axis initially
  std::vector<int64_t> output_dims(axes.size() + input_shape.NumDimensions(), 0);

  // Set all axes indices to 1 in output_dims and check for duplicates
  for (int64_t axis : axes) {
    // Valid axis range is [0, output_rank - 1]
    axis = HandleNegativeAxis(axis, output_dims.size());
    if (axis < 0 || axis >= static_cast<int64_t>(output_dims.size()))
      return Status(ONNXRUNTIME, INVALID_ARGUMENT, "'axes' has an out of range axis");
    if (output_dims[axis] != 0)
      return Status(ONNXRUNTIME, INVALID_ARGUMENT, "'axes' has a duplicate axis");
    output_dims[axis] = 1;
  }

  // Now fill in the zero entries with the existing shape
  {
    auto begin = input_shape.GetDims().cbegin();
    for (auto& axisSize : output_dims) {
      if (axisSize == 0)
        axisSize = *begin++;
    }
    assert(begin == input_shape.GetDims().cend());
  }

  output_shape = TensorShape(output_dims);
  return Status::OK();
}

Status Unsqueeze::Compute(OpKernelContext* ctx) const {
  const auto* input_tensor = ctx->Input<Tensor>(0);
  ORT_ENFORCE(input_tensor != nullptr);
  TensorShape output_shape;
  ORT_RETURN_IF_ERROR(PrepareCompute(ctx, input_tensor->Shape(), output_shape));
  auto* output_tensor = ctx->Output(0, output_shape);
  ORT_RETURN_IF(nullptr == output_tensor, "Failed to get output tensor");
  CopyCpuTensor(input_tensor, output_tensor);
  return Status::OK();
}
}  // namespace onnxruntime
