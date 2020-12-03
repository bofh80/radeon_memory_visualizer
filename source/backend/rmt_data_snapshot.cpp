//=============================================================================
/// Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author
/// \brief Functions working on a snapshot.
//=============================================================================

#include "rmt_data_snapshot.h"
#include "rmt_resource_history.h"
#include "rmt_data_set.h"
#include <rmt_assert.h>
#include <rmt_print.h>
#include "rmt_address_helper.h"
#include <stdlib.h>  // for free()

#ifndef _WIN32
#include "linux/safe_crt.h"
#endif

static void DumpResourceList(FILE* file, RmtResource** resources, int32_t resource_count)
{
    fprintf(file, "\t\t\t\"Resources\" : [\n");

    for (int32_t current_resource_index = 0; current_resource_index < resource_count; ++current_resource_index)
    {
        const RmtResource* current_resource = resources[current_resource_index];

        fprintf(file, "\t\t\t\t\"Resource %llu\" : {\n", (unsigned long long)current_resource->identifier);
        fprintf(file, "\t\t\t\t\tCreated : %llu,\n", (unsigned long long)current_resource->create_time);
        fprintf(file, "\t\t\t\t\tBind : %llu,\n", (unsigned long long)current_resource->bind_time);
        fprintf(file, "\t\t\t\t\tAddress : \"0x%010llx\",\n", (unsigned long long)current_resource->address);
        fprintf(file, "\t\t\t\t\tSize (Bytes) : %lld,\n", (unsigned long long)current_resource->size_in_bytes);
        fprintf(file, "\t\t\t\t\tType : \"%s\"\n", RmtGetResourceTypeNameFromResourceType(current_resource->resource_type));
        fprintf(file, "\t\t\t\t}");

        if (current_resource_index < (resource_count - 1))
        {
            fprintf(file, ",\n");
        }
        else
        {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "\t\t\t]\n");
}

static void DumpAllocationList(FILE* file, const RmtVirtualAllocationList* allocation_list)
{
    for (int32_t current_allocation_index = 0; current_allocation_index < allocation_list->allocation_count; ++current_allocation_index)
    {
        const RmtVirtualAllocation* allocation_details = &allocation_list->allocation_details[current_allocation_index];

        fprintf(file, "\t\"Allocation %d\" : {\n", allocation_details->guid);
        fprintf(file, "\t\t \"Address\" : \"0x%010llx\",\n", (unsigned long long)allocation_details->base_address);
        fprintf(file, "\t\t \"Size\" : %lld,\n", (unsigned long long)RmtGetAllocationSizeInBytes(allocation_details->size_in_4kb_page, kRmtPageSize4Kb));
        fprintf(file, "\t\t \"Created\" : %lld,\n", (unsigned long long)allocation_details->timestamp);
        fprintf(file, "\t\t \"Last CPU map\" : %lld,\n", (unsigned long long)allocation_details->last_cpu_map);
        fprintf(file, "\t\t \"Last CPU unmap\" : %lld,\n", (unsigned long long)allocation_details->last_cpu_un_map);
        fprintf(file, "\t\t \"Last residency update\" : %lld,\n", (unsigned long long)allocation_details->last_residency_update);
        fprintf(file, "\t\t \"Map count\" : %d,\n", allocation_details->map_count);
        fprintf(file, "\t\t \"Unbound regions\" : %d,\n", allocation_details->unbound_memory_region_count);
        fprintf(file, "\t\t \"Resource count\" : %d,\n", allocation_details->resource_count);

        if (allocation_details->resource_count > 0)
        {
            DumpResourceList(file, allocation_details->resources, allocation_details->resource_count);
        }

        fprintf(file, "\t}");

        if (current_allocation_index < (allocation_list->allocation_count - 1))
        {
            fprintf(file, ",\n");
        }
        else
        {
            fprintf(file, "\n");
        }
    }
}

// dump the state.
RmtErrorCode RmtDataSnapshotDumpJsonToFile(const RmtDataSnapshot* snapshot, const char* filename)
{
    FILE*   file     = NULL;
    errno_t error_no = fopen_s(&file, filename, "wb");
    if ((file == nullptr) || error_no != 0)
    {
        return RMT_ERROR_FILE_NOT_OPEN;
    }

    DumpAllocationList(file, &snapshot->virtual_allocation_list);

    fflush(file);
    fclose(file);
    return RMT_OK;
}

// do the first pass over the RMT data, figure out the resource-based events, and
// virtual memory-based events, and also build a list of physical address ranges
// that the resource interacts with during its life. This list will be used in the
// 2nd pass of the algorithm.
static RmtErrorCode ProcessTokensIntoResourceHistory(RmtDataSet* data_set, const RmtResource* resource, RmtResourceHistory* out_resource_history)
{
    // Reset the RMT stream parsers ready to load the data.
    RmtStreamMergerReset(&data_set->stream_merger);

    while (!RmtStreamMergerIsEmpty(&data_set->stream_merger))
    {
        // grab the next token from the heap.
        RmtToken     current_token;
        RmtErrorCode error_code = RmtStreamMergerAdvance(&data_set->stream_merger, &current_token);
        RMT_ASSERT(error_code == RMT_OK);

        // interested in tokens that directly reference resources.
        switch (current_token.type)
        {
        case kRmtTokenTypeResourceCreate:
            if (current_token.resource_create_token.resource_identifier != resource->identifier)
            {
                break;
            }

            RmtResourceHistoryAddEvent(
                out_resource_history, kRmtResourceHistoryEventResourceCreated, current_token.common.thread_id, current_token.common.timestamp, false);
            break;

        case kRmtTokenTypeResourceDestroy:
            if (current_token.resource_destroy_token.resource_identifier != resource->identifier)
            {
                break;
            }

            RmtResourceHistoryAddEvent(
                out_resource_history, kRmtResourceHistoryEventResourceDestroyed, current_token.common.thread_id, current_token.common.timestamp, false);
            break;

        case kRmtTokenTypeResourceBind:
            if (current_token.resource_bind_token.resource_identifier != resource->identifier)
            {
                break;
            }

            RmtResourceHistoryAddEvent(
                out_resource_history, kRmtResourceHistoryEventResourceBound, current_token.common.thread_id, current_token.common.timestamp, false);
            break;

        case kRmtTokenTypeVirtualAllocate:
        {
            const RmtGpuAddress address_of_last_byte_allocation =
                (current_token.virtual_allocate_token.virtual_address + current_token.virtual_allocate_token.size_in_bytes) - 1;
            if (!RmtResourceOverlapsVirtualAddressRange(resource, current_token.virtual_allocate_token.virtual_address, address_of_last_byte_allocation))
            {
                break;
            }

            RmtResourceHistoryAddEvent(
                out_resource_history, kRmtResourceHistoryEventVirtualMemoryAllocated, current_token.common.thread_id, current_token.common.timestamp, false);
        }
        break;

        case kRmtTokenTypeResourceReference:

            if (out_resource_history->base_allocation == nullptr)
            {
                break;
            }

            // NOTE: PAL can only make resident/evict a full virtual allocation on CPU, not just a single resource.
            if (current_token.cpu_map_token.virtual_address != out_resource_history->base_allocation->base_address)
            {
                break;
            }

            if (current_token.resource_reference.residency_update_type == kRmtResidencyUpdateTypeAdd)
            {
                RmtResourceHistoryAddEvent(out_resource_history,
                                           kRmtResourceHistoryEventVirtualMemoryMakeResident,
                                           current_token.common.thread_id,
                                           current_token.common.timestamp,
                                           false);
            }
            else
            {
                RmtResourceHistoryAddEvent(
                    out_resource_history, kRmtResourceHistoryEventVirtualMemoryEvict, current_token.common.thread_id, current_token.common.timestamp, false);
            }
            break;

        case kRmtTokenTypeCpuMap:

            if (out_resource_history->base_allocation == nullptr)
            {
                break;
            }

            // NOTE: PAL can only map/unmap a full virtual allocation on CPU, not just a resource.
            if (current_token.cpu_map_token.virtual_address != out_resource_history->base_allocation->base_address)
            {
                break;
            }

            if (current_token.cpu_map_token.is_unmap)
            {
                RmtResourceHistoryAddEvent(
                    out_resource_history, kRmtResourceHistoryEventVirtualMemoryUnmapped, current_token.common.thread_id, current_token.common.timestamp, false);
            }
            else
            {
                RmtResourceHistoryAddEvent(
                    out_resource_history, kRmtResourceHistoryEventVirtualMemoryMapped, current_token.common.thread_id, current_token.common.timestamp, false);
            }
            break;

        case kRmtTokenTypeVirtualFree:
        {
            if (out_resource_history->base_allocation == nullptr)
            {
                break;
            }

            if (!RmtResourceOverlapsVirtualAddressRange(
                    resource, current_token.virtual_free_token.virtual_address, (current_token.virtual_free_token.virtual_address + 1)))
            {
                break;
            }

            RmtResourceHistoryAddEvent(
                out_resource_history, kRmtResourceHistoryEventVirtualMemoryFree, current_token.common.thread_id, current_token.common.timestamp, false);
        }
        break;

        case kRmtTokenTypePageTableUpdate:
        {
            if (out_resource_history->base_allocation == nullptr)
            {
                break;
            }

            // check for overlap between the resource VA range and this change to the PA mappings.
            const uint64_t size_in_bytes =
                RmtGetAllocationSizeInBytes(current_token.page_table_update_token.size_in_pages, current_token.page_table_update_token.page_size);

            if (!RmtAllocationsOverlap(current_token.page_table_update_token.virtual_address,
                                       size_in_bytes,
                                       out_resource_history->resource->address,
                                       out_resource_history->resource->size_in_bytes))
            {
                break;
            }

            if (current_token.page_table_update_token.is_unmapping)
            {
                RmtResourceHistoryAddEvent(
                    out_resource_history, kRmtResourceHistoryEventPhysicalUnmap, current_token.common.thread_id, current_token.common.timestamp, true);
            }
            else
            {
                if (current_token.page_table_update_token.physical_address == 0)
                {
                    RmtResourceHistoryAddEvent(
                        out_resource_history, kRmtResourceHistoryEventPhysicalMapToHost, current_token.common.thread_id, current_token.common.timestamp, true);
                }
                else
                {
                    RmtResourceHistoryAddEvent(
                        out_resource_history, kRmtResourceHistoryEventPhysicalMapToLocal, current_token.common.thread_id, current_token.common.timestamp, true);
                }
            }
        }
        break;

        default:
            break;
        }
    }

    return RMT_OK;
}

// Helper functo call the correct free function.
static void PerformFree(RmtDataSet* data_set, void* pointer)
{
    if (data_set->free_func == nullptr)
    {
        return free(pointer);
    }

    return (data_set->free_func)(pointer);
}

RmtErrorCode RmtDataSnapshotGenerateResourceHistory(RmtDataSnapshot* snapshot, const RmtResource* resource, RmtResourceHistory* out_resource_history)
{
    RMT_ASSERT(snapshot);
    RMT_ASSERT(resource);
    RMT_ASSERT(out_resource_history);
    RMT_RETURN_ON_ERROR(snapshot, RMT_ERROR_INVALID_POINTER);
    RMT_RETURN_ON_ERROR(resource, RMT_ERROR_INVALID_POINTER);
    RMT_RETURN_ON_ERROR(out_resource_history, RMT_ERROR_INVALID_POINTER);

    // stash the pointer to the resource and the underlaying VA.
    out_resource_history->resource        = resource;
    out_resource_history->base_allocation = resource->bound_allocation;
    out_resource_history->event_count     = 0;

    RmtErrorCode error_code = RMT_OK;

    error_code = ProcessTokensIntoResourceHistory(snapshot->data_set, resource, out_resource_history);
    RMT_RETURN_ON_ERROR(error_code == RMT_OK, error_code);

    return RMT_OK;
}

RmtErrorCode RmtDataSnapshotDestroy(RmtDataSnapshot* snapshot)
{
    RMT_RETURN_ON_ERROR(snapshot, RMT_ERROR_INVALID_POINTER);
    RMT_RETURN_ON_ERROR(snapshot->data_set, RMT_ERROR_MALFORMED_DATA);

    // free the memory allocated for the snapshot.
    PerformFree(snapshot->data_set, snapshot->virtual_allocation_buffer);
    PerformFree(snapshot->data_set, snapshot->resource_list_buffer);
    PerformFree(snapshot->data_set, snapshot->region_stack_buffer);

    return RMT_OK;
}

uint64_t RmtDataSnapshotGetLargestResourceSize(const RmtDataSnapshot* snapshot)
{
    RMT_RETURN_ON_ERROR(snapshot, 0);

    uint64_t latest_resource_size_in_bytes = 0;

    for (int32_t current_resource_index = 0; current_resource_index < snapshot->resource_list.resource_count; ++current_resource_index)
    {
        const RmtResource* current_resource = &snapshot->resource_list.resources[current_resource_index];
        latest_resource_size_in_bytes       = RMT_MAXIMUM(latest_resource_size_in_bytes, current_resource->size_in_bytes);
    }

    return latest_resource_size_in_bytes;
}

/// Get the smallest resource size (in bytes) seen in a snapshot.
uint64_t RmtDataSnapshotGetSmallestResourceSize(const RmtDataSnapshot* snapshot)
{
    RMT_RETURN_ON_ERROR(snapshot, 0);

    uint64_t smallest_resource_size_in_bytes = UINT64_MAX;

    for (int32_t current_resource_index = 0; current_resource_index < snapshot->resource_list.resource_count; ++current_resource_index)
    {
        const RmtResource* current_resource = &snapshot->resource_list.resources[current_resource_index];
        smallest_resource_size_in_bytes     = RMT_MINIMUM(smallest_resource_size_in_bytes, current_resource->size_in_bytes);
    }

    if (smallest_resource_size_in_bytes == UINT64_MAX)
    {
        return 0;
    }

    return smallest_resource_size_in_bytes;
}

RmtErrorCode RmtDataSnapshotGetSegmentStatus(const RmtDataSnapshot* snapshot, RmtHeapType heap_type, RmtSegmentStatus* out_segment_status)
{
    RMT_RETURN_ON_ERROR(snapshot, RMT_ERROR_INVALID_POINTER);
    RMT_RETURN_ON_ERROR(out_segment_status, RMT_ERROR_INVALID_POINTER);

    out_segment_status->heap_type = heap_type;

    // all this stuff is flagged.
    switch (heap_type)
    {
    case kRmtHeapTypeInvisible:
        out_segment_status->flags |= kRmtSegmentStatusFlagVram;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuVisible;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuCached;
        break;
    case kRmtHeapTypeLocal:
        out_segment_status->flags |= kRmtSegmentStatusFlagVram;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuVisible;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuCached;
        out_segment_status->flags |= kRmtSegmentStatusFlagCpuVisible;
        break;
    case kRmtHeapTypeSystem:
        out_segment_status->flags |= kRmtSegmentStatusFlagHost;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuVisible;
        out_segment_status->flags |= kRmtSegmentStatusFlagGpuCached;
        out_segment_status->flags |= kRmtSegmentStatusFlagCpuVisible;
        out_segment_status->flags |= kRmtSegmentStatusFlagCpuCached;
        break;

    default:
        break;
    }

    out_segment_status->total_physical_size        = snapshot->data_set->segment_info[heap_type].size;
    out_segment_status->total_bound_virtual_memory = 0;

    // calculate data for the segment info fields.
    uint64_t max_virtual_allocation_size      = 0;
    uint64_t min_virtual_allocation_size      = UINT64_MAX;
    uint64_t total_virtual_memory_requested   = 0;
    uint64_t total_physical_mapped_by_process = snapshot->page_table.mapped_per_heap[heap_type];
    ;
    uint64_t allocation_count = 0;

    // set the resource committed memory values.
    for (int32_t current_resource_usage_index = 0; current_resource_usage_index < kRmtResourceUsageTypeCount; ++current_resource_usage_index)
    {
        out_segment_status->physical_bytes_per_resource_usage[current_resource_usage_index] = 0;
    }

    for (int32_t current_virtual_allocation_index = 0; current_virtual_allocation_index < snapshot->virtual_allocation_list.allocation_count;
         ++current_virtual_allocation_index)
    {
        const RmtVirtualAllocation* current_virtual_allocation = &snapshot->virtual_allocation_list.allocation_details[current_virtual_allocation_index];

        const uint64_t size_in_bytes = RmtGetAllocationSizeInBytes(current_virtual_allocation->size_in_4kb_page, kRmtPageSize4Kb);

        if (current_virtual_allocation->heap_preferences[0] == heap_type)
        {
            total_virtual_memory_requested += size_in_bytes;
            max_virtual_allocation_size = RMT_MAXIMUM(max_virtual_allocation_size, size_in_bytes);
            min_virtual_allocation_size = RMT_MINIMUM(min_virtual_allocation_size, size_in_bytes);
            allocation_count++;
        }

        // walk each resource in the allocation and work out what heap each resource is in.
        for (int32_t current_resource_index = 0; current_resource_index < current_virtual_allocation->resource_count; ++current_resource_index)
        {
            const RmtResource* current_resource = current_virtual_allocation->resources[current_resource_index];

            if (current_resource->resource_type == kRmtResourceTypeHeap)
            {
                continue;
            }

            // process the resource
            const RmtResourceUsageType current_resource_usage = RmtResourceGetUsageType(current_resource);

            if (current_virtual_allocation->heap_preferences[0] == heap_type)
            {
                out_segment_status->total_bound_virtual_memory += current_resource->size_in_bytes;
            }

            // caculate the histogram of where each resource has its memory committed.
            uint64_t resource_histogram[kRmtResourceBackingStorageCount] = {0};
            RmtResourceGetBackingStorageHistogram(snapshot, current_resource, resource_histogram);
            out_segment_status->physical_bytes_per_resource_usage[current_resource_usage] += resource_histogram[heap_type];
        }
    }

    if (min_virtual_allocation_size == UINT64_MAX)
    {
        min_virtual_allocation_size = 0;
    }

    // fill out the structure fields.
    out_segment_status->total_virtual_memory_requested           = total_virtual_memory_requested;
    out_segment_status->total_physical_mapped_by_process         = total_physical_mapped_by_process;
    out_segment_status->total_physical_mapped_by_other_processes = 0;
    out_segment_status->max_allocation_size                      = max_virtual_allocation_size;
    out_segment_status->min_allocation_size                      = min_virtual_allocation_size;
    if (allocation_count > 0)
    {
        out_segment_status->mean_allocation_size = total_virtual_memory_requested / allocation_count;
    }
    else
    {
        out_segment_status->mean_allocation_size = 0;
    }

    return RMT_OK;
}

// calculate the seg. status.
RmtSegmentSubscriptionStatus RmtSegmentStatusGetOversubscribed(const RmtSegmentStatus* segment_status)
{
    const uint64_t close_limit = (uint64_t)((double)segment_status->total_physical_size * 0.8);
    if (segment_status->total_virtual_memory_requested > segment_status->total_physical_size)
    {
        return kRmtSegmentSubscriptionStatusOverLimit;
    }
    if (segment_status->total_virtual_memory_requested > close_limit)
    {
        return kRmtSegmentSubscriptionStatusCloseToLimit;
    }

    return kRmtSegmentSubscriptionStatusUnderLimit;
}

// get the heap type for a physical address.
RmtHeapType RmtDataSnapshotGetSegmentForAddress(const RmtDataSnapshot* snapshot, RmtGpuAddress gpu_address)
{
    RMT_RETURN_ON_ERROR(snapshot, kRmtHeapTypeUnknown);

    // special case for system memory.
    if (gpu_address == 0)
    {
        return kRmtHeapTypeSystem;
    }

    for (int32_t current_segment_index = 0; current_segment_index < snapshot->data_set->segment_info_count; ++current_segment_index)
    {
        const RmtGpuAddress start_address = snapshot->data_set->segment_info->base_address;
        const RmtGpuAddress end_address =
            snapshot->data_set->segment_info[current_segment_index].base_address + snapshot->data_set->segment_info[current_segment_index].size;

        if (start_address <= gpu_address && gpu_address < end_address)
        {
            return snapshot->data_set->segment_info[current_segment_index].heap_type;
        }
    }

    return kRmtHeapTypeUnknown;
}
