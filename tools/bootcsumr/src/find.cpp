#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

#include <cstdio>
#include <cstdlib>

#include <stdint.h>

#include <string>
#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <streambuf>

static const size_t PLATFORM_INDEX = 0;
static const size_t DEVICE_INDEX = 0;

static const size_t W = 1024 * 64;
static const size_t H = 1024 * 64;

const cl::NDRange kernelRangeGlobal(W, H);
const cl::NDRange kernelRangeLocal(256, 1);

#define kernelFile "src/find.cl"
#define kernelName "find"


// type1
#define MAGIC1 (((0x6c078965ULL * 0x3f) & 0xffffffff) + 1)
#define KEY1 0x45CC73EE317AULL
// type2
#define MAGIC2 (((0x6c078965ULL * 0x3f) & 0xffffffff) + 1)
#define KEY2 0xA536C0F1D859ULL
// type3
#define MAGIC3 (((0x6c078965ULL * 0x78) & 0xffffffff) + 1)
#define KEY3 0x586FD4709867ULL
// type5
#define MAGIC5 (((0x6c078965ULL * 0x91) & 0xffffffff) + 1)
#define KEY5 0x8618A45BC2D3ULL
// type6
#define MAGIC6 (((0x6c078965ULL * 0x85) & 0xffffffff) + 1)
#define KEY6 0x2BBAD4E6EB74ULL

static inline uint32_t checksum_helper (uint32_t op1, uint32_t op2, uint32_t op3) {
    int low_mult;
    int high_mult;

    if (op2 == 0) {
        op2 = op3;
    }

    low_mult = ((uint64_t)op1 * (uint64_t)op2) & 0x00000000FFFFFFFF;
    high_mult = (((uint64_t)op1 * (uint64_t)op2) & 0xFFFFFFFF00000000) >> 32;

    if (high_mult - low_mult == 0) {
        return low_mult;
    } else {
        return high_mult - low_mult;
    }
}

static inline void first(uint32_t frame[], uint32_t prev_inst, uint32_t bcode_inst, uint32_t loop_count) {
    frame[0] += checksum_helper(0x3EF - loop_count, bcode_inst, loop_count);
    frame[1] = checksum_helper(frame[1], bcode_inst, loop_count);
    frame[2] ^= bcode_inst;
    frame[3] += checksum_helper(bcode_inst + 5, 0x6c078965, loop_count);
    if (prev_inst < bcode_inst) {
        frame[9] = checksum_helper(frame[9], bcode_inst, loop_count);
    } else {
        frame[9] += bcode_inst;
    }
    frame[4] += ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
    frame[7] = checksum_helper(frame[7], ((bcode_inst >> (0x20 - (prev_inst & 0x1f))) | (bcode_inst << (prev_inst & 0x1f))), loop_count);
    if (bcode_inst < frame[6]) {
        frame[6] = (bcode_inst + loop_count) ^ (frame[3] + frame[6]);
    } else {
        frame[6] = (frame[4] + bcode_inst) ^ frame[6];
    }
    frame[5] += (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27));
    frame[8] = checksum_helper(frame[8], (bcode_inst << (0x20 - (prev_inst >> 27))) | (bcode_inst >> (prev_inst >> 27)), loop_count);
}

static inline void second(uint32_t frame[], uint32_t prev_inst, uint32_t bcode_inst, uint32_t next_inst, uint32_t loop_count) {
    uint32_t tmp1 = checksum_helper(frame[15], (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27)), loop_count);
    frame[15] = checksum_helper(
        tmp1,
        (next_inst << (bcode_inst >> 27)) | (next_inst >> (0x20 - (bcode_inst >> 27))),
        loop_count
    );
    uint32_t tmp2 = ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
    uint32_t tmp3 = checksum_helper(frame[14], tmp2, loop_count); // v0 at 1384
    uint32_t tmp4 = checksum_helper(tmp3, (next_inst >> (bcode_inst & 0x1f)) | (next_inst << (0x20 - (bcode_inst & 0x1f))), loop_count); // v0 at 13a4
    frame[14] = tmp4;
    frame[13] += ((bcode_inst >> (bcode_inst & 0x1f)) | (bcode_inst << (0x20 - (bcode_inst & 0x1f)))) + ((next_inst >> (next_inst & 0x1f)) | (next_inst << (0x20 - (next_inst & 0x1f))));
    frame[10] = checksum_helper(frame[10] + bcode_inst, next_inst, loop_count);
    frame[11] = checksum_helper(frame[11] ^ bcode_inst, next_inst, loop_count);
    frame[12] += (frame[8] ^ bcode_inst);
}

static bool find256(uint32_t &hword, uint32_t &word, const uint64_t desired_checksum, const uint32_t preframes[16*256], const uint32_t prev_inst, const uint32_t bcode_inst) {
    cl_int err = CL_SUCCESS;
    try {
        // Platforms
        // --------------------------------------------
        std::cout << "=======================================" << std::endl;
        std::cout << "Platforms" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::vector<cl::Platform> platforms;
        err |= cl::Platform::get(&platforms);
        for (auto &plat : platforms) {
            std::array<std::string, 5> params {{
                plat.getInfo<CL_PLATFORM_PROFILE>(),
                plat.getInfo<CL_PLATFORM_VERSION>(),
                plat.getInfo<CL_PLATFORM_NAME>(),
                plat.getInfo<CL_PLATFORM_VENDOR>(),
                plat.getInfo<CL_PLATFORM_EXTENSIONS>(),
            }};
            for (auto &param : params) {
                std::cout << param << std::endl;
            }

            std::cout << "--------------------" << std::endl;
        }
        if (platforms.size() == 0 || PLATFORM_INDEX >= platforms.size()) {
            std::cout << "ERROR: No platforms" << std::endl;
            return false;
        }
        std::cout << std::endl;

        cl::Platform &plat = platforms[PLATFORM_INDEX];
        std::cout << "Use platform " << PLATFORM_INDEX << std::endl;
        std::cout << std::endl;


        // Devices & Context
        // --------------------------------------------
        std::cout << "=======================================" << std::endl;
        std::cout << "Devices" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::vector<cl::Device> devices;
        err |= plat.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        size_t i = 0;
        for (auto &device : devices) {
            std::cout << "--------------------" << std::endl;
            std::cout << i << std::endl;
            std::cout << "--------------------" << std::endl;

            std::array<std::string, 14> params {{
                device.getInfo<CL_DEVICE_NAME>(),
                device.getInfo<CL_DEVICE_VENDOR>(),
                device.getInfo<CL_DEVICE_PROFILE>(),
                device.getInfo<CL_DEVICE_VERSION>(),
                device.getInfo<CL_DRIVER_VERSION>(),
                device.getInfo<CL_DEVICE_OPENCL_C_VERSION>(),
                std::to_string(device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()) + "[Cores] @ " + std::to_string(device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>()) + "[MHz]",
                "Work item: " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>()) + ", " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[0]) + ", " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[1]) + ", " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[2]),
                "Work group: " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()),
                "Constant: " + std::to_string(device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>()),
#if CL_HPP_TARGET_OPENCL_VERSION < 200
                "Host Unified Memory: " + std::to_string(device.getInfo<CL_DEVICE_HOST_UNIFIED_MEMORY>()),
                "CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE: " + std::string{ "?" },
                "CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE: " + std::string{ "?" },
#else
                "Host Unified Memory: " + std::string{ "?" },
                "CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE>()),
                "CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE>()),
#endif
                device.getInfo<CL_DEVICE_EXTENSIONS>(),
            }};
            for (auto &param : params) {
                std::cout << param << std::endl;
            }
            ++i;
        }
        if (devices.size() == 0) {
            std::cout << "ERROR: No devices" << std::endl;
            return false;
        }
        std::cout << std::endl;

        i = DEVICE_INDEX;
        cl::Device &device = devices[i];
        std::cout << "Use device " << i << std::endl;
        std::cout << std::endl;
        // Context
        cl::Context context(device);


        // Build
        // --------------------------------------------
        std::ifstream from(kernelFile);
        std::string kernelStr((std::istreambuf_iterator<char>(from)),
                               std::istreambuf_iterator<char>());
        from.close();
        cl::Program::Sources sources {kernelStr};
        cl::Program program = cl::Program(context, sources);
        try {
            err |= program.build("");
        } catch (cl::Error &err) {
            std::cerr
            << "ERROR: "
            << err.what()
            << "("
            << err.err()
            << ")"
            << std::endl;

            cl_int buildErr = CL_SUCCESS;
            auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
            for (auto &pair : buildInfo) {
                std::cerr << pair.second << std::endl;
            }
            return false;
        }

        // Execution
        // --------------------------------------------
        cl::CommandQueue queue(
            context,
            cl::QueueProperties::None //cl::QueueProperties::Profiling
        );

        auto kernelFunc = cl::KernelFunctor<cl::Buffer, cl::Buffer, uint64_t, uint32_t, uint32_t, uint32_t>(program, kernelName);
        auto kernel = kernelFunc.getKernel();
        size_t s = kernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(device);
        std::cout << "CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: " << s << std::endl;

        uint32_t result[64] = {0,};
        cl::Buffer resultDev(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t)*64);
        err |= cl::copy(queue, result, result+64, resultDev);
        cl::Buffer preframesDev(context, CL_MEM_READ_ONLY, sizeof(uint32_t)*16*256);
        err |= cl::copy(queue, preframes, preframes+16*256, preframesDev);

        constexpr uint32_t Z = 256;
        for (uint32_t z = 0; z < Z; z++) {
            kernelFunc(
                cl::EnqueueArgs(
                    queue,
                    kernelRangeGlobal,
                    kernelRangeLocal
                ),
                resultDev,
                preframesDev,
                static_cast<uint64_t>(desired_checksum),
                static_cast<uint32_t>(prev_inst),
                static_cast<uint32_t>(bcode_inst),
                z,
                err
            );

            //queue.flush();
            std::cout << "z: " << z << "/" << Z << std::endl;
        }
        queue.finish();
        // dump
        {
            err |= cl::copy(queue, resultDev, result, result+64);

            hword = result[2];
            word = result[1];
            return result[0] != 0;
        }
    }
    catch (cl::Error &err) {
        std::cerr
        << "ERROR: "
        << err.what()
        << "("
        << err.err()
        << ")"
        << std::endl;
    }

    return false;
}

/*
 * Try to find checksum collision
 */
extern "C" bool find_collision (uint32_t *bcode, int type) {
    uint32_t magic = bcode[0];
    uint64_t desired_checksum;
    switch (type) {
    case 1:
        magic ^= MAGIC1;
        desired_checksum = KEY1;
        break;
    case 2:
        magic ^= MAGIC2;
        desired_checksum = KEY2;
        break;
    case 3:
        magic ^= MAGIC3;
        desired_checksum = KEY3;
        break;
    case 5:
        magic ^= MAGIC5;
        desired_checksum = KEY5;
        break;
    case 6:
        magic ^= MAGIC6;
        desired_checksum = KEY6;
        break;
    default:
        return false;
        break;
    }

    // Generate frame. This is done earlier in IPC2
    // Pre-calculated frame, up to what changes
    uint32_t preframes[16 * 256];
    for (int i = 0; i < 16; i ++) {
        preframes[i] = magic;
    };

    // Calculate pre-frame
    // Loop start, 11E8 - 11FC
    bcode[-1] = bcode[0]; // 1st prev
    int32_t i = 0;
    for (; i < 0x3ed; i++) {
        first(preframes, bcode[i-1], bcode[i], i+1);
        second(preframes, bcode[i-1], bcode[i], bcode[i+1], i+1);
    }
    first(preframes, bcode[i-1], bcode[i], i+1);

    ::memcpy(preframes+16, preframes, 16*4);
    ::memcpy(preframes+32, preframes, 32*4);
    ::memcpy(preframes+64, preframes, 64*4);
    ::memcpy(preframes+128, preframes, 128*4);

    // Store starting hword into bootcode
    // TODO: loop (starthword & 0xff00)
    uint32_t starthword = 0x00000000;
    uint32_t bcode_0x3ee[256];
    for (uint32_t i = 0; i < 256; i++) {
        bcode_0x3ee[i] = (bcode[0x3ee] & 0xffff0000) | starthword | i;
        // Frame calculations for 0x3ed
        second(preframes+16*i, bcode[0x3ec], bcode[0x3ed], bcode_0x3ee[i], 0x3ed + 1);
        // Frame calculations for 0x3ee
        first(preframes+16*i, bcode[0x3ed], bcode_0x3ee[i], 0x3ee + 1);
    }

    // Now let's try everything for the last word
    // Current frame being used to test
    uint32_t hword = 0;
    uint32_t word = 0;
    bool found = false;
    found = find256(hword, word, desired_checksum, preframes, bcode[0x3ed], bcode_0x3ee[0]);
    if (found) {
        // Write word to end of bootcode
        bcode[0x3ee] = hword;
        bcode[0x3ef] = word;
    }
    return found;
}
