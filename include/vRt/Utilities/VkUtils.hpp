#pragma once

//#include "../Parts/Headers.inl"

#include <vulkan/vulkan.hpp> // only for inner usage
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <array>
#include <map>
#include <random>
#include <vector>
#include <algorithm>
#include <execution>
#include <iterator>
#include <cstddef>

// don't be so stupid in C++20 area
//#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace _vt {
    const int64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

    // read binary (for SPIR-V)
    std::vector<char> readBinary(std::string filePath) {
        std::ifstream file(filePath, std::ios::in | std::ios::binary | std::ios::ate);
        std::vector<char> data;
        if (file.is_open())
        {
            std::streampos size = file.tellg();
            data.resize(size);
            file.seekg(0, std::ios::beg);
            file.read(&data[0], size);
            file.close();
        }
        else
        {
            std::cerr << "Failure to open " + filePath << std::endl;
        }
        return data;
    };

    // shader pipeline barrier
    void shaderBarrier(const VkCommandBuffer& cmdBuffer) {
        VkMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 
            1, &memoryBarrier,
            0, nullptr, 
            0, nullptr);
    };

    // general command buffer barrier
    void commandBarrier(const VkCommandBuffer& cmdBuffer) {
        VkMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, 
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
            1, &memoryBarrier,
            0, nullptr, 
            0, nullptr);
    };

    // create secondary command buffers for batching compute invocations
    auto createCommandBuffer(const VkDevice device, const VkCommandPool cmdPool) {
        VkCommandBuffer cmdBuffer;

        VkCommandBufferAllocateInfo cmdi;
        cmdi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdi.commandPool = cmdPool;
        cmdi.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        cmdi.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &cmdi, &cmdBuffer);

        VkCommandBufferInheritanceInfo inhi;
        inhi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inhi.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

        VkCommandBufferBeginInfo bgi;
        bgi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //bgi.flags = 0;
        //bgi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        bgi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        bgi.pInheritanceInfo = &inhi;
        vkBeginCommandBuffer(cmdBuffer, &bgi);

        return cmdBuffer;
    };

    // create shader module
    auto loadAndCreateShaderModule(VkDevice device, std::string path) {
        auto code = readBinary(path);
        VkShaderModuleCreateInfo smi{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr , 0, code.size(), (uint32_t *)code.data() };
        VkShaderModule sm; vkCreateShaderModule(device, &smi, nullptr, &sm);
        return sm;
    }

    // create compute pipelines
    auto createCompute(VkDevice device, std::string path, VkPipelineLayout layout, VkPipelineCache cache){
        auto module = loadAndCreateShaderModule(device, path);

        VkPipelineShaderStageCreateInfo spi;
        spi.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        spi.module = module;
        spi.pName = "main";
        spi.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        VkComputePipelineCreateInfo cmpi;
        cmpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cmpi.layout = layout;
        cmpi.stage = spi;

        VkPipeline pipeline;
        vkCreateComputePipelines(device, cache, 1, &cmpi, nullptr, &pipeline);
        return pipeline;
    }

    // add dispatch in command buffer (with default pipeline barrier)
    void cmdDispatch(VkCommandBuffer cmd, VkPipeline pipeline, uint32_t x = 1, uint32_t y = 1, uint32_t z = 1){
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdDispatch(cmd, x, y, z);
        shaderBarrier(cmd); // put shader barrier
    }

    // submit command (with async wait)
    void submitCmdAsync(VkDevice device, VkQueue queue, std::vector<VkCommandBuffer> cmds, std::function<void()> asyncCallback = {}){
        // no commands 
        if (cmds.size() <= 0) return;

        VkSubmitInfo smbi;
        smbi.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        smbi.waitSemaphoreCount = 0;
        smbi.signalSemaphoreCount = 0;
        smbi.commandBufferCount = cmds.size();
        smbi.pCommandBuffers = cmds.data();

        VkFence fence; VkFenceCreateInfo fin{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
        vkCreateFence(device, &fin, nullptr, &fence);
        vkQueueSubmit(queue, 1, &smbi, fence);
        std::async(std::launch::async | std::launch::deferred, [=]() {
            vkWaitForFences(device, 1, &fence, true, DEFAULT_FENCE_TIMEOUT);
            std::async(std::launch::async | std::launch::deferred, [=]() {
                vkDestroyFence(device, fence, nullptr);
                asyncCallback();
            });
        });
    }


    uint32_t VtIdentifier = 0x1FFu;
    struct VkShortHead {
        uint32_t sublevel : 23, identifier : 9;
        const void * pNext;
    };

    struct VkFullHead {
        uint32_t sType;
        const void * pNext;
    };


    template <class VtS>
    const VkFullHead* vtSearchStructure(VtS& structure, VtStructureType sType) {
        VkFullHead* head = (VkFullHead*)&structure;
        VkFullHead* found = nullptr;
        for (int i = 0; i < 255; i++) {
            if (!head) break;
            if (head->sType == sType) {
                found = head; break;
            }
            head = (VkFullHead*)head->pNext;
        }
        return found;
    };

    template <class VtS>
    const VkShortHead* vtExplodeArtificals(VtS& structure) {
        VkShortHead* head = (VkShortHead*)&structure;
        VkShortHead* lastVkStructure = nullptr;
        VkShortHead* firstVkStructure = nullptr;
        for (int i = 0; i < 255;i++) {
            if (!head) break;
            if (head->identifier != VtIdentifier) {
                if (lastVkStructure) {
                    lastVkStructure->pNext = head;
                } else {
                    firstVkStructure = head;
                }
                lastVkStructure = head;
            }
            head = (VkShortHead*)head->pNext;
        }
        return firstVkStructure;
    };

};
