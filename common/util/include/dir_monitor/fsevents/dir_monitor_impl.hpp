//
// FSEvents-based implementation for OSX
//
// Apple documentation about FSEvents interface:
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/
//     Reference/reference.html#//apple_ref/doc/c_ref/kFSEventStreamCreateFlagFileEvents
//
// Copyright (c) 2014 Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <string>
#include <deque>
#include <boost/thread.hpp>
#include <CoreServices/CoreServices.h>
#define DIR_MONITOR_USE_DISPATCH_QUEUE 1

namespace boost {
namespace asio {

class dir_monitor_impl
{
public:
    dir_monitor_impl() :
        run_(true),
#ifdef DIR_MONITOR_USE_DISPATCH_QUEUE
        queue_(dispatch_queue_create("DirectoryMonitor", DISPATCH_QUEUE_SERIAL)),
#else
        work_thread_(&boost::asio::dir_monitor_impl::work_thread, this),
#endif
        fsevents_(nullptr)
    {}

    ~dir_monitor_impl()
    {
#ifdef DIR_MONITOR_USE_DISPATCH_QUEUE
        stop_fsevents();
        dispatch_release(queue_);
#else
        stop_work_thread();
        work_thread_.join();
        stop_fsevents();
#endif
    }

    void add_directory(const boost::filesystem::path& dirname)
    {
        boost::unique_lock<boost::mutex> lock(dirs_mutex_);
        dirs_.insert(dirname.native());//@todo Store path in dictionary
        stop_fsevents();
        start_fsevents();
    }

    void remove_directory(const boost::filesystem::path& dirname)
    {
        boost::unique_lock<boost::mutex> lock(dirs_mutex_);
        dirs_.erase(dirname.native());
        stop_fsevents();
        start_fsevents();
    }

    bool already_added_directory(const boost::filesystem::path& dirname)
    {
        boost::unique_lock<boost::mutex> lock(dirs_mutex_);
        return (dirs_.count(dirname.native()));
    }

    void destroy()
    {
        boost::unique_lock<boost::mutex> lock(events_mutex_);
        run_ = false;
        events_cond_.notify_all();
    }

    dir_monitor_event popfront_event(boost::system::error_code &ec)
    {
        boost::unique_lock<boost::mutex> lock(events_mutex_);
        while (run_ && events_.empty())
        {
            events_cond_.wait(lock);
        }
        dir_monitor_event ev;
        if (!events_.empty())
        {
            ec = boost::system::error_code();
            ev = events_.front();
            events_.pop_front();
        }
        else {
            ec = boost::asio::error::operation_aborted;
        }
        return ev;
    }

    void pushback_event(dir_monitor_event ev)
    {
        boost::unique_lock<boost::mutex> lock(events_mutex_);
        if (run_)
        {
            events_.push_back(ev);
            events_cond_.notify_all();
        }
    }

private:
    CFArrayRef make_array(const boost::unordered_set<std::string>& in)
    {
        CFMutableArrayRef arr = CFArrayCreateMutable(kCFAllocatorDefault, in.size(), &kCFTypeArrayCallBacks);
        for (const std::string& str : in) {
            CFStringRef cfstr = CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
            CFArrayAppendValue(arr, cfstr);
            // @todo CFRelease(cfstr); ??
        }
        return arr;
    }

    void start_fsevents()
    {
        if (dirs_.size() == 0) {
            fsevents_ = nullptr;
            return;
        }

        FSEventStreamContext context = {0, this, NULL, NULL, NULL};
        fsevents_ =
            FSEventStreamCreate(
                kCFAllocatorDefault,
                &boost::asio::dir_monitor_impl::fsevents_callback,
                &context,
                make_array(dirs_),
                kFSEventStreamEventIdSinceNow, /* only new modifications */
                //(CFTimeInterval)5.0, /* 5 seconds latency interval */
                (CFTimeInterval)0.0, //added by BT
                kFSEventStreamCreateFlagFileEvents
                | kFSEventStreamCreateFlagNoDefer //added by BT
            );
        FSEventStreamRetain(fsevents_);

        if (!fsevents_)
        {
//As of Boost v1.66.0, get_system_category was deprecated with language: "Boost.System deprecates the old names, but will provide them when the macro BOOST_SYSTEM_ENABLE_DEPRECATED is defined."
//Prior versions <= 1.65.1 it said: "Boost.System deprecates the old names, but continues to provide them unless macro BOOST_SYSTEM_NO_DEPRECATED is defined."
#if (BOOST_VERSION < 106600)
            boost::system::system_error e(boost::system::error_code(errno, boost::system::get_system_category()), "boost::asio::dir_monitor_impl::init_kqueue: kqueue failed");
#else
            boost::system::system_error e(boost::system::error_code(errno, boost::system::system_category()), "boost::asio::dir_monitor_impl::init_kqueue: kqueue failed");
#endif
            boost::throw_exception(e);
        }


#ifdef DIR_MONITOR_USE_DISPATCH_QUEUE
        FSEventStreamSetDispatchQueue(fsevents_, queue_);
        FSEventStreamStart(fsevents_);
#else
        while (!runloop_) {
            boost::this_thread::yield();
        }
        FSEventStreamScheduleWithRunLoop(fsevents_, runloop_, kCFRunLoopDefaultMode);
        FSEventStreamStart(fsevents_);
        runloop_cond_.notify_all();
#endif
        FSEventStreamFlushAsync(fsevents_);
    }

    void stop_fsevents()
    {
        if (fsevents_)
        {
            FSEventStreamStop(fsevents_);
            // FSEventStreamUnscheduleFromRunLoop(fsevents_, runloop_, kCFRunLoopDefaultMode);
            FSEventStreamInvalidate(fsevents_);
            FSEventStreamRelease(fsevents_);
        }
    }

    static void fsevents_callback(
            ConstFSEventStreamRef streamRef,
            void *clientCallBackInfo,
            size_t numEvents,
            void *eventPaths,
            const FSEventStreamEventFlags eventFlags[],
            const FSEventStreamEventId eventIds[])
    {
        (void)streamRef;
        (void)eventIds;
        char **paths = (char**)eventPaths;
        dir_monitor_impl* impl = (dir_monitor_impl*)clientCallBackInfo;

        for (size_t i = 0; i < numEvents; ++i)
        {
            boost::filesystem::path dir(paths[i]);
            //dir is an absolute file path with all symbolic links resolved
            if (eventFlags[i] & kFSEventStreamEventFlagMustScanSubDirs) {
                impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::recursive_rescan));
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
                if (!impl->already_added_directory(dir)) { //fix to ignore when creating directory right before adding to the monitor
                    impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::added));
                }
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
                impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::removed));
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
                impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::modified));
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed)
            {
                if (!boost::filesystem::exists(dir))
                {
                    impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::renamed_old_name));
                }
                else
                {
                    impl->pushback_event(dir_monitor_event(dir, dir_monitor_event::renamed_new_name));
                }
            }
       }
    }

#ifndef DIR_MONITOR_USE_DISPATCH_QUEUE
    void work_thread()
    {
        runloop_ = CFRunLoopGetCurrent();

        while (running())
        {
            boost::unique_lock<boost::mutex> lock(runloop_mutex_);
            runloop_cond_.wait(lock);
            CFRunLoopRun();
        }
    }

    bool running()
    {
        boost::unique_lock<boost::mutex> lock(work_thread_mutex_);
        return run_;
    }

    void stop_work_thread()
    {
        // Access to run_ is sychronized with running().
        boost::mutex::scoped_lock lock(work_thread_mutex_);
        run_ = false;
        CFRunLoopStop(runloop_); // exits the thread
        runloop_cond_.notify_all();
    }

    bool run_;
    CFRunLoopRef runloop_;
    boost::mutex runloop_mutex_;
    boost::condition_variable runloop_cond_;

    boost::mutex work_thread_mutex_;
    boost::thread work_thread_;
#else
    bool run_;
    dispatch_queue_t queue_; //added by BT
#endif

    FSEventStreamRef fsevents_;

    boost::mutex dirs_mutex_;
    boost::unordered_set<std::string> dirs_;

    boost::mutex events_mutex_;
    boost::condition_variable events_cond_;
    std::deque<dir_monitor_event> events_;
};

} // asio namespace
} // boost namespace

