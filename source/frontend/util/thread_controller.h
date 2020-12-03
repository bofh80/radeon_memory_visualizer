//=============================================================================
/// Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Header for a thread controller. The thread controller is used to
/// manage work done on a separate thread so as to not lock up the UI (main)
/// thread. Responsible for setting up and starting the worker thread and
/// starting and stopping the loading animation in the UI thread while the work
/// is done in the worker thread
//=============================================================================

#ifndef RMV_UTIL_THREAD_CONTROLLER_H_
#define RMV_UTIL_THREAD_CONTROLLER_H_

#include <QWidget>
#include <QThread>

class MainWindow;

namespace rmv
{
    /// The base class for a background task. This is the object that will be run
    /// from the thread_controller. Custom jobs can inherit from this class
    /// and implement the ThreadFunc() function.
    class BackgroundTask : public QObject
    {
        Q_OBJECT

    public:
        /// Constructor.
        BackgroundTask();

        /// Destructor.
        ~BackgroundTask();

        /// Implement these in derived classes.
        virtual void ThreadFunc() = 0;

        /// The function that runs the thread. Calls the derived
        /// ThreadFunc() and cleans up afterwards.
        void Start();

    signals:
        /// Indicate that initial processing of the pane has completed.
        void WorkerFinished();
    };

    class ThreadController : public QObject
    {
        Q_OBJECT

    public:
        /// Constructor
        /// Will take ownership of the worker thread and delete it later.
        explicit ThreadController(MainWindow* main_window, QWidget* parent, BackgroundTask* worker);

        /// Destructor
        virtual ~ThreadController();

        /// This is run in the main thread once the worker thread has finished.
        void WorkerFinished();

        /// Has the thread finished.
        /// \return true if finished, false if not.
        bool Finished() const;

    signals:
        /// Indicate that the worker thread has finished.
        void ThreadFinished();

    private:
        MainWindow*     main_window_;      ///< Pointer to the main window.
        QThread*        thread_;           ///< The worker thread.
        BackgroundTask* background_task_;  ///< The worker object that does the work.
        bool            finished_;         ///< Is the data valid.
    };
}  // namespace rmv

#endif  // RMV_UTIL_THREAD_CONTROLLER_H_
