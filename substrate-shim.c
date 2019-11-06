#include <stdlib.h>
#include <objc/runtime.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <stdio.h>

extern void *SubGetImageByName(const char *filename) __asm__("SubGetImageByName");;
void *MSGetImageByName(const char *filename) {
	    return SubGetImageByName(filename);
}

extern void *SubFindSymbol(void *image, const char *name) __asm__("SubFindSymbol");
void *MSFindSymbol(void *image, const char *name) {
		return SubFindSymbol(image, name);
}

extern void SubHookFunction(void *symbol, void *replace, void **result) __asm__("SubHookFunction");
void MSHookFunction(void *symbol, void *replace, void **result) {
		SubHookFunction(symbol, replace, result);
}

extern void SubHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result) __asm__("SubHookMessageEx");
void MSHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result) {
		SubHookMessageEx(_class, sel, imp, result);
}

void MSHookMemory(void *target, const void *data, size_t size) {
    uintptr_t page_start = (uintptr_t)target & ~PAGE_MASK;
    uintptr_t page_end = ((uintptr_t)target + size - 1) & ~PAGE_MASK;
    size_t len = page_end - page_start + PAGE_SIZE;
    vm_address_t region = (vm_address_t)page_start;
    vm_size_t region_len = 0;
    struct vm_region_submap_short_info_64 info;
    mach_msg_type_number_t info_count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
    natural_t max_depth = 99999;
    kern_return_t kr = vm_region_recurse_64(mach_task_self(), &region, &region_len, &max_depth, (vm_region_recurse_info_t)&info, &info_count);
    if (kr == KERN_SUCCESS) {
        vm_address_t copy = 0;
        kr = vm_map(mach_task_self(), &copy, len, PAGE_MASK, VM_FLAGS_ANYWHERE, MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_ALL, VM_INHERIT_SHARE | VM_INHERIT_COPY);
        if (kr == KERN_SUCCESS) {
            kr = vm_copy(mach_task_self(), page_start, len, copy);
            if (kr == KERN_SUCCESS) {
                vm_prot_t cur = 0;
                vm_prot_t max = 0;
                vm_address_t address = page_start;
                kr = vm_remap(mach_task_self(), &address, len, 0, VM_FLAGS_OVERWRITE, mach_task_self(), copy, TRUE, &cur, &max, info.protection);
                if (kr == KERN_SUCCESS) {
                    memcpy(target, data, size);
                    kr = vm_protect(mach_task_self(), address, len, FALSE, info.inheritance);
                    if (kr != KERN_SUCCESS) {
                        mach_error("vm_protect", kr);
                    }
                } else {
                    mach_error("vm_remap", kr);
                }
            } else {
                mach_error("vm_copy", kr);
            }
            kr = vm_deallocate(mach_task_self(), copy, len);
            if (kr != KERN_SUCCESS) {
                mach_error("vm_deallocate", kr);
            }
        } else {
            mach_error("vm_map", kr);
        }
    } else {
        mach_error("vm_region_recurse_64", kr);
    }
    return;
}

// i don't think anyone uses this function anymore, but it's here for completeness
void MSHookClassPair(Class _class, Class hook, Class old) {
    unsigned int n_methods = 0;
    Method *hooks = class_copyMethodList(hook, &n_methods);

    for (unsigned int i = 0; i < n_methods; ++i) {
        SEL selector = method_getName(hooks[i]);
        const char *what = method_getTypeEncoding(hooks[i]);

        Method old_mptr = class_getInstanceMethod(old, selector);
        Method cls_mptr = class_getInstanceMethod(_class, selector);

        if (cls_mptr) {
            class_addMethod(old, selector, method_getImplementation(hooks[i]), what);
            method_exchangeImplementations(cls_mptr, old_mptr);
        } else {
            class_addMethod(_class, selector, method_getImplementation(hooks[i]), what);
        }
    }

    free(hooks);
}
