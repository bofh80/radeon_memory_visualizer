//=============================================================================
/// Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Model implementation for Allocation explorer pane
//=============================================================================

#include "models/snapshot/allocation_explorer_model.h"

#include <QTableView>
#include <QScrollBar>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "qt_common/utils/scaling_manager.h"

#include "rmt_assert.h"
#include "rmt_data_set.h"
#include "rmt_data_snapshot.h"
#include "rmt_print.h"

#include "models/trace_manager.h"

namespace rmv
{
    VirtualAllocationExplorerModel::VirtualAllocationExplorerModel(int32_t num_allocation_models)
        : ModelViewMapper(kVirtualAllocationExplorerNumWidgets)
        , allocation_table_model_(nullptr)
        , resource_table_model_(nullptr)
        , allocation_proxy_model_(nullptr)
        , resource_proxy_model_(nullptr)
        , resource_thresholds_{}
        , minimum_allocation_size_(0)
        , maximum_allocation_size_(0)
    {
        allocation_bar_model_ = new AllocationBarModel(num_allocation_models, false);
    }

    VirtualAllocationExplorerModel::~VirtualAllocationExplorerModel()
    {
        delete allocation_bar_model_;
        delete allocation_table_model_;
        delete resource_table_model_;
        delete allocation_proxy_model_;
        delete resource_proxy_model_;
    }

    void VirtualAllocationExplorerModel::ResetModelValues()
    {
        allocation_table_model_->removeRows(0, allocation_table_model_->rowCount());
        allocation_table_model_->SetRowCount(0);
        resource_table_model_->removeRows(0, resource_table_model_->rowCount());
        resource_table_model_->SetRowCount(0);
        allocation_proxy_model_->invalidate();
        resource_proxy_model_->invalidate();
        memset(resource_thresholds_, 0, sizeof(uint64_t) * (kSizeSliderRange + 1));
        allocation_bar_model_->ClearSelectionState();
    }

    bool VirtualAllocationExplorerModel::OpenSnapshot(const RmtDataSnapshot* snapshot)
    {
        if (snapshot->virtual_allocation_list.allocation_count <= 0)
        {
            return false;
        }

        allocation_bar_model_->ClearSelectionState();
        return true;
    }

    void VirtualAllocationExplorerModel::UpdateAllocationTable()
    {
        const TraceManager& trace_manager = TraceManager::Get();
        if (!trace_manager.DataSetValid())
        {
            return;
        }

        const RmtDataSnapshot* open_snapshot = trace_manager.GetOpenSnapshot();

        allocation_table_model_->removeRows(0, allocation_table_model_->rowCount());

        minimum_allocation_size_ = UINT64_MAX;
        maximum_allocation_size_ = 0;

        // update allocation table and min/max allocation sizes
        int32_t allocation_count = open_snapshot->virtual_allocation_list.allocation_count;
        allocation_table_model_->SetRowCount(allocation_count);
        for (int32_t allocation_index = 0; allocation_index < allocation_count; allocation_index++)
        {
            const RmtVirtualAllocation* virtual_allocation = &open_snapshot->virtual_allocation_list.allocation_details[allocation_index];
            allocation_table_model_->AddAllocation(open_snapshot, virtual_allocation);
            uint64_t allocation_size = RmtVirtualAllocationGetSizeInBytes(virtual_allocation);
            if (allocation_size < minimum_allocation_size_)
            {
                minimum_allocation_size_ = allocation_size;
            }
            if (allocation_size > maximum_allocation_size_)
            {
                maximum_allocation_size_ = allocation_size;
            }
        }
        allocation_proxy_model_->invalidate();
    }

    int32_t VirtualAllocationExplorerModel::UpdateResourceTable()
    {
        int resource_count = 0;

        const TraceManager& trace_manager = TraceManager::Get();
        if (!trace_manager.DataSetValid())
        {
            return resource_count;
        }

        const RmtVirtualAllocation* selected_allocation = allocation_bar_model_->GetAllocation(0, 0);
        if (selected_allocation == nullptr)
        {
            return resource_count;
        }

        resource_table_model_->removeRows(0, resource_table_model_->rowCount());

        // update resource table
        const RmtDataSnapshot* open_snapshot = trace_manager.GetOpenSnapshot();
        resource_count                       = selected_allocation->resource_count;
        resource_table_model_->SetRowCount(resource_count);
        for (int32_t i = 0; i < resource_count; i++)
        {
            resource_table_model_->AddResource(open_snapshot, selected_allocation->resources[i], kSnapshotCompareIdUndefined);
        }
        resource_proxy_model_->invalidate();
        return resource_count;
    }

    void VirtualAllocationExplorerModel::InitializeAllocationTableModel(ScaledTableView* table_view, uint num_rows, uint num_columns)
    {
        RMT_ASSERT(allocation_proxy_model_ == nullptr);
        allocation_proxy_model_ = new AllocationProxyModel();
        allocation_table_model_ = allocation_proxy_model_->InitializeAllocationTableModels(table_view, num_rows, num_columns);

        table_view->horizontalHeader()->setSectionsClickable(true);

        table_view->SetColumnPadding(0);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnId, 9);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnAllocationSize, 10);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnBound, 8);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnUnbound, 8);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnAverageResourceSize, 12);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnResourceSizeStdDev, 15);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnResourceCount, 11);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnPreferredHeapName, 11);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnInvisiblePercentage, 13);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnLocalPercentage, 11);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnSystemPercentage, 11);
        table_view->SetColumnWidthEms(kVirtualAllocationColumnUnmappedPercentage, 8);

        table_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);
    }

    void VirtualAllocationExplorerModel::InitializeResourceTableModel(ScaledTableView* table_view, uint num_rows, uint num_columns)
    {
        RMT_ASSERT(resource_proxy_model_ == nullptr);
        resource_proxy_model_ = new ResourceProxyModel();
        resource_table_model_ = resource_proxy_model_->InitializeResourceTableModels(table_view, num_rows, num_columns);
        resource_table_model_->Initialize(table_view, false);
    }

    void VirtualAllocationExplorerModel::AllocationSearchBoxChanged(const QString& filter)
    {
        allocation_proxy_model_->SetSearchFilter(filter);
        allocation_proxy_model_->invalidate();
    }

    void VirtualAllocationExplorerModel::AllocationSizeFilterChanged(int min_value, int max_value) const
    {
        const uint64_t diff       = maximum_allocation_size_ - minimum_allocation_size_;
        const uint64_t scaled_min = ((min_value * diff) / kSizeSliderRange) + minimum_allocation_size_;
        const uint64_t scaled_max = ((max_value * diff) / kSizeSliderRange) + minimum_allocation_size_;

        allocation_proxy_model_->SetSizeFilter(scaled_min, scaled_max);
        allocation_proxy_model_->invalidate();
    }

    void VirtualAllocationExplorerModel::ResourceSearchBoxChanged(const QString& filter)
    {
        resource_proxy_model_->SetSearchFilter(filter);
        resource_proxy_model_->invalidate();
    }

    void VirtualAllocationExplorerModel::ResourceSizeFilterChanged(int min_value, int max_value)
    {
        if (allocation_bar_model_->GetAllocation(0, 0) != nullptr)
        {
            const uint64_t scaled_min = resource_thresholds_[min_value];
            const uint64_t scaled_max = resource_thresholds_[max_value];

            resource_proxy_model_->SetSizeFilter(scaled_min, scaled_max);
            resource_proxy_model_->invalidate();
        }
    }

    AllocationProxyModel* VirtualAllocationExplorerModel::GetAllocationProxyModel() const
    {
        return allocation_proxy_model_;
    }

    ResourceProxyModel* VirtualAllocationExplorerModel::GetResourceProxyModel() const
    {
        return resource_proxy_model_;
    }

    void VirtualAllocationExplorerModel::BuildResourceSizeThresholds()
    {
        std::vector<uint64_t> resource_sizes;

        const RmtVirtualAllocation* selected_allocation = allocation_bar_model_->GetAllocation(0, 0);

        if (selected_allocation != nullptr && selected_allocation->resource_count > 0)
        {
            int32_t resource_count = selected_allocation->resource_count;
            resource_sizes.reserve(resource_count);
            for (int current_resource_index = 0; current_resource_index < selected_allocation->resource_count; current_resource_index++)
            {
                resource_sizes.push_back(selected_allocation->resources[current_resource_index]->size_in_bytes);
            }
            TraceManager::Get().BuildResourceSizeThresholds(resource_sizes, resource_thresholds_);
        }
    }

    AllocationBarModel* VirtualAllocationExplorerModel::GetAllocationBarModel() const
    {
        return allocation_bar_model_;
    }
}  // namespace rmv