//=============================================================================
/// Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Model header for the Allocation overview pane
//=============================================================================

#ifndef RMV_MODELS_SNAPSHOT_ALLOCATION_OVERVIEW_MODEL_H_
#define RMV_MODELS_SNAPSHOT_ALLOCATION_OVERVIEW_MODEL_H_

#include "qt_common/utils/model_view_mapper.h"

#include "rmt_virtual_allocation_list.h"

#include "models/allocation_multi_bar_model.h"

namespace rmv
{
    /// Enum containing indices for the widgets shared between the model and UI.
    enum AllocationOverviewWidgets
    {
        kAllocationOverviewNumWidgets,
    };

    class AllocationOverviewModel : public ModelViewMapper
    {
    public:
        /// Sort modes available for memory allocations. Each sort mode should have a sort function.
        /// The order here is the order the sort modes will be in the combo box (default at the top).
        enum SortMode
        {
            kSortModeAllocationSize,
            kSortModeAllocationID,
            kSortModeAllocationAge,
            kSortModeResourceCount,
            kSortModeFragmentationScore,

            kSortModeCount
        };

        /// Sort direction (ascending or descending).
        /// The order here is the order the sort directions will be in the combo box (default at the top).
        enum SortDirection
        {
            kSortDirectionDescending,
            kSortDirectionAscending,

            kSortDirectionCount
        };

        /// Constructor.
        /// \param num_allocation_models The number of models to represent the allocations.
        explicit AllocationOverviewModel(int32_t num_allocation_models);

        /// Destructor.
        virtual ~AllocationOverviewModel();

        /// Initialize blank data for the model.
        void ResetModelValues();

        /// Sort the allocations.
        /// \param sort_mode The sort mode to sort by.
        /// \param ascending Whether to use ascending or descending ordering.
        void Sort(int sort_mode, bool ascending);

        /// Apply filters and rebuild the list of allocations.
        /// \param filter_text The search text specified in the UI.
        /// \param heap_array_flags An array of flags indicating if the corresponding heap
        /// should be shown.
        void ApplyFilters(const QString& filter_text, const bool* heap_array_flags);

        /// Get the number of viewable allocations. These are the allocations that can
        /// be seen in the scene and are the ones which pass all the text and heap filtering
        /// tests.
        /// \return The number of viewable allocations.
        size_t GetViewableAllocationCount() const;

        /// Set whether the allocations should be normalized.
        /// \param normalized true if the allocations should be normalized, false if not.
        void SetNormalizeAllocations(bool normalized);

        /// Select a resource on this pane. This is usually called when selecting a resource
        /// on a different pane to make sure the resource selection is propagated to all
        /// interested panes.
        /// \param resource_identifier the resource identifier of the resource to select.
        /// \param model_index The index of the model referred to. This pane uses a single model
        /// to represent all the allocations.
        /// \return The index in the scene of the selected resource.
        size_t SelectResource(RmtResourceIdentifier resource_identifier, int32_t model_index);

        /// Get the model for the allocation bar.
        /// \return The allocation bar model.
        MultiAllocationBarModel* GetAllocationBarModel() const;

    private:
        MultiAllocationBarModel*          allocation_bar_model_;  ///< The model for the allocation bar graphs.
        AllocationOverviewModel::SortMode sort_mode_;             ///< The sort mode to use for the comparison.
        bool                              sort_ascending_;        ///< If true, use ascending sort. Otherwise descending.
    };
}  // namespace rmv

#endif  // RMV_MODELS_SNAPSHOT_ALLOCATION_OVERVIEW_MODEL_H_
