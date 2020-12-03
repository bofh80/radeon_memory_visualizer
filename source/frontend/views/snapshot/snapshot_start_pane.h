//=============================================================================
/// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Header for the Snapshot start pane.
//=============================================================================

#ifndef RMV_VIEWS_SNAPSHOT_SNAPSHOT_START_PANE_H_
#define RMV_VIEWS_SNAPSHOT_SNAPSHOT_START_PANE_H_

#include "ui_snapshot_start_pane.h"

#include <QGraphicsScene>

#include "views/base_pane.h"
#include "views/custom_widgets/rmv_camera_snapshot_widget.h"

/// Class declaration
class SnapshotStartPane : public BasePane
{
    Q_OBJECT

public:
    /// constructor.
    /// \param parent The widget's parent.
    explicit SnapshotStartPane(QWidget* parent = nullptr);

    /// Destructor.
    virtual ~SnapshotStartPane();

    /// Overridden window resize event.
    /// \param event the resize event object.
    virtual void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;

    /// Reset UI state.
    virtual void Reset() Q_DECL_OVERRIDE;

    /// Update UI coloring.
    virtual void ChangeColoring() Q_DECL_OVERRIDE;

    /// Open a snapshot.
    /// \param snapshot Pointer to the snapshot to open.
    virtual void OpenSnapshot(RmtDataSnapshot* snapshot) Q_DECL_OVERRIDE;

private slots:
    /// Callback for when the DPI Scale changes.
    void OnScaleFactorChanged();

private:
    /// Resizes the GraphicsView to fit the scene.
    void ResizeGraphicsView();

    Ui::SnapshotStartPane*   ui_;               ///< Pointer to the Qt UI design.
    QGraphicsScene*          scene_;            ///< Qt scene for the camera drawing.
    RMVCameraSnapshotWidget* snapshot_widget_;  ///< Circle with camera.
};

#endif  // RMV_VIEWS_SNAPSHOT_SNAPSHOT_START_PANE_H_
