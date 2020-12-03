//=============================================================================
/// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Main entry point.
//=============================================================================

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <stdarg.h>

#include "rmt_print.h"
#include "rmt_data_set.h"
#include "rmt_data_snapshot.h"

#include "qt_common/utils/scaling_manager.h"

#include "models/trace_manager.h"
#include "views/main_window.h"
#include "util/rmv_util.h"

/// Handle printing from RMV backend.
/// \param pMsg Incoming message
void PrintCallback(const char* message)
{
    DebugWindow::DbgMsg(message);
}

/// Detect RMV trace if any was specified as command line param
/// \return Empty string if no trace, and full path if valid RMV file
static QString GetTracePath()
{
    QString out = "";

    if (QCoreApplication::arguments().count() > 1)
    {
        const QString potential_trace_path = QDir::toNativeSeparators(QCoreApplication::arguments().at(1));
        if (TraceManager::Get().TraceValidToLoad(potential_trace_path) == true)
        {
            out = potential_trace_path;
        }
    }

    return out;
}

#if 0
static RmtDataSet command_line_data_set;
static RmtDataSnapshot command_line_snapshot;
#endif

/// Main entry point.
/// \param argc The number of arguments.
/// \param argv An array containing arguments.
int main(int argc, char* argv[])
{
#if 0
    // Test feature to dump RMV file to JSON. Need a bunch of error handling.
    if (argc == 4)
    {
        RmtErrorCode error_code = rmtDataSetInitialize(argv[1], &command_line_data_set);
        if (error_code != RMT_OK)
        {
            return 1;
        }

        const uint64_t timestamp = strtoull(argv[2], NULL, 0);
        RMT_UNUSED(timestamp);
        error_code = rmtDataSetGenerateSnapshot(&command_line_data_set, command_line_data_set.maximumTimestamp, "snapshot 0", &command_line_snapshot);
        if (error_code != RMT_OK)
        {
            return 2;
        }

        error_code = rmtDataSnapshotDumpJsonToFile(&command_line_snapshot, argv[3]);
        if (error_code != RMT_OK)
        {
            return 3;
        }

        return 0;
    }
#endif

    QApplication a(argc, argv);

    // Load application stylesheet
    QFile style_sheet(rmv::resource::kStylesheet);
    if (style_sheet.open(QFile::ReadOnly))
    {
        a.setStyleSheet(style_sheet.readAll());
    }

    // set the default font size
    QFont font;
    font.setFamily(font.defaultFamily());
    font.setPointSize(8);
    a.setFont(font);

    MainWindow::InitializeJobQueue();
    MainWindow* window = new (std::nothrow) MainWindow();
    int         result = -1;
    if (window != nullptr)
    {
        window->show();

        TraceManager::Get().Initialize(window);

        // Scaling manager object registration
        ScalingManager::Get().Initialize(window);

        if (!GetTracePath().isEmpty())
        {
            window->LoadTrace(GetTracePath());
        }

        result = a.exec();
        delete window;
    }

    return result;
}
