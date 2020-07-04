#ifndef SYCL_INCLUDE_CL_SYCL_NEC_VE_KERNEL_HPP_
#define SYCL_INCLUDE_CL_SYCL_NEC_VE_KERNEL_HPP_

#include "CL/SYCL/nec/offload_driver.hpp"

namespace cl::sycl::detail {

struct VEKernel : public Kernel {
  nec::VEProc proc;
  nec::VEContext ctx;

  VEKernel(const vector_class<KernelArg> &args, const nec::VEProc &proc) : Kernel(args), proc(proc) {
    ctx = nec::ctx_create(proc);
  }

  vector_class<uint64_t> copy_in(struct veo_args *argp) {
    vector_class<uint64_t> ve_addr_list;

    for (int i = 0; i < args.size(); i++) {
      KernelArg arg = args[i];
      size_t size_in_byte = arg.container->get_size();

      uint64_t ve_addr_int;
      int rt = veo_alloc_mem(proc.ve_proc, &ve_addr_int, size_in_byte);
      if (rt != veo_command_state::VEO_COMMAND_OK) {
        DEBUG_INFO("[VEProc] allocate VE memory size: {} failed, return code: {}", size_in_byte, rt);
        throw nec::VEException("VE allocate return error");
      }
      ve_addr_list.push_back(ve_addr_int);

      DEBUG_INFO("[VEKernel] allocate ve memory, size: {}, ve address: {:#x}",
                 size_in_byte,
                 ve_addr_int
      );

      if (arg.mode != access::mode::write) {
        DEBUG_INFO("[VEKernel] do copy to ve memory for arg, device address: {:#x}, size: {}, host address: {:#x}",
                   (size_t) ve_addr_int,
                   size_in_byte,
                   (size_t) arg.container->get_data_ptr()
        );
        rt = veo_write_mem(proc.ve_proc, ve_addr_int, arg.container->get_data_ptr(), size_in_byte);
        if (rt != veo_command_state::VEO_COMMAND_OK) {
          DEBUG_INFO("[VEProc] copy to ve memory failed, size: {}, return code: {}", size_in_byte, rt);
          throw nec::VEException("VE copy return error");
        }
      }
      veo_args_set_i64(argp, i, ve_addr_int);
    }
    return ve_addr_list;
  }

  void copy_out(vector_class<uint64_t> ve_addr_list) {
    for (int i = 0; i < args.size(); i++) {
      KernelArg arg = args[i];
      size_t size_in_byte = arg.container->get_size();
      uint64_t device_ptr = ve_addr_list[i];
      if (arg.mode == access::mode::read) {
        // do not copy back for read only
        continue;
      }
      DEBUG_INFO("[VEKernel] copy from ve memory, device address: {:#x}, size: {}, host address: {:#x}",
                 (size_t) device_ptr,
                 size_in_byte,
                 (size_t) arg.container->get_data_ptr()
      );
      // do copy
      int rt = veo_read_mem(proc.ve_proc, arg.container->get_data_ptr(), device_ptr, size_in_byte);
      if (rt != veo_command_state::VEO_COMMAND_OK) {
        DEBUG_INFO("[VEProc] copy from ve memory failed, size: {}, return code: {}", size_in_byte, rt);
        throw nec::VEException("VE copy return error");
      }
    }
  }

  void single_task(string_class kernel_name) override {
    DEBUG_INFO("[VEKernel] single task: {}", kernel_name);

    veo_args *argp = nec::create_ve_args();
    DEBUG_INFO("[VEKernel] create ve args: {:#x}", (size_t) argp);

    try {

      vector_class<uint64_t> ve_addr_list = copy_in(argp);
      DEBUG_INFO("[VEKernel] invoke ve func: {}", kernel_name);
      uint64_t id = veo_call_async_by_name(ctx.ve_ctx, proc.handle, kernel_name.c_str(), argp);
      uint64_t ret_val;
      veo_call_wait_result(ctx.ve_ctx, id, &ret_val);
      DEBUG_INFO("[VEKernel] ve func finished, id: {}, ret val: {:#x}", id, ret_val);
      copy_out(ve_addr_list);

    } catch (nec::VEException &e) {
      std::cerr << "[VEKernel] kernel invoke failed, error message: " << e.what() << std::endl;
    }

    veo_args_free(argp);

  }
  void parallel_for(const range<1> &r, string_class kernel_name) override {
    DEBUG_INFO("[VEKernel] parallel_for 1d");
  }
  void parallel_for(const range<2> &r, string_class kernel_name) override {
    DEBUG_INFO("[VEKernel] parallel_for 2d");
  }
  void parallel_for(const range<3> &r, string_class kernel_name) override {
    DEBUG_INFO("[VEKernel] parallel_for 3d");
  }

  virtual ~VEKernel() {
    nec::free_ctx(ctx);
  }

};

}

#endif //SYCL_INCLUDE_CL_SYCL_NEC_VE_KERNEL_HPP_
