using System;
using System.Threading;
using Agebull.Common.ApiDocuments;
using Agebull.Common.Logging;
using Newtonsoft.Json;
using ZeroMQ;

namespace Agebull.ZeroNet.Core
{
    /// <summary>
    /// 系统侦听器
    /// </summary>
    public static class SystemMonitor
    {
        #region 网络处理

        private static readonly SemaphoreSlim TaskEndSem = new SemaphoreSlim(0);

        /// <summary>
        /// 等待正常结束
        /// </summary>
        internal static void WaitMe()
        {
            if (ZeroApplication.WorkModel == ZeroWorkModel.Service)
                TaskEndSem.Wait();
        }

        /// <summary>
        ///     进入系统侦听
        /// </summary>
        internal static void Monitor()
        {
            if (ZeroApplication.WorkModel != ZeroWorkModel.Service)
                return;

            using (OnceScope.CreateScope(ZeroApplication.Config))
            {
            }
            ZeroTrace.SystemLog("Zero center in monitor...");
            TaskEndSem.Release();
            while (ZeroApplication.IsAlive)
            {
                if (MonitorInner())
                    continue;
                ZeroApplication.ZerCenterStatus = ZeroCenterState.Failed;
                ZeroApplication.OnZeroEnd();
                ZeroApplication.ApplicationState = StationState.Failed;
                Thread.Sleep(1000);
            }

            TaskEndSem.Release();
        }

        /// <summary>
        ///     进入系统侦听
        /// </summary>
        static bool MonitorInner()
        {
            DateTime failed = DateTime.MinValue;
            using (var poll = ZmqPool.CreateZmqPool())
            {
                poll.Prepare(new[] { ZSocket.CreateSubSocket(ZeroApplication.Config.ZeroMonitorAddress, ZeroIdentityHelper.CreateIdentity()) }, ZPollEvent.In);
                while (ZeroApplication.IsAlive)
                {
                    if (!poll.Poll() || !poll.CheckIn(0, out var message))
                    {
                        if (failed == DateTime.MinValue)
                            failed = DateTime.Now;
                        else if ((DateTime.Now - failed).TotalMinutes > 1)
                            return false;
                        continue;
                    }
                    failed = DateTime.MinValue;
                    if (!message.Unpack(out var item))
                        continue;
                    OnMessagePush(item.ZeroEvent, item.SubTitle, item.Content);
                }
            }
            return true;
        }
        #endregion

        #region 事件处理

        /// <summary>
        ///     收到信息的处理
        /// </summary>
        private static void OnMessagePush(ZeroNetEventType zeroNetEvent, string station, string content)
        {
            if (ZeroApplication.ZerCenterStatus == ZeroCenterState.Failed ||
                ZeroApplication.ApplicationState == StationState.Failed)
            {
                ZeroApplication.JoinCenter();
                return;
            }
            switch (zeroNetEvent)
            {
                case ZeroNetEventType.CenterSystemStart:
                    center_start(content);
                    return;
                case ZeroNetEventType.CenterSystemClosing:
                    center_closing(content);
                    return;
                case ZeroNetEventType.CenterSystemStop:
                    center_stop(content);
                    return;
                case ZeroNetEventType.CenterWorkerSoundOff:
                    worker_sound_off();
                    return;
            }
            if (!ZeroApplication.InRun)
                return;
            switch (zeroNetEvent)
            {
                case ZeroNetEventType.CenterStationState:
                    station_state(station, content);
                    return;
                case ZeroNetEventType.CenterStationInstall:
                    station_install(station, content);
                    return;
                case ZeroNetEventType.CenterStationUpdate:
                    station_update(station, content);
                    return;
                case ZeroNetEventType.CenterStationJoin:
                    station_join(station, content);
                    return;
                case ZeroNetEventType.CenterStationPause:
                    station_pause(station);
                    return;
                case ZeroNetEventType.CenterStationResume:
                    station_resume(station);
                    return;
                case ZeroNetEventType.CenterStationClosing:
                    station_closing(station);
                    return;
                case ZeroNetEventType.CenterStationLeft:
                    station_left(station);
                    return;
                case ZeroNetEventType.CenterStationStop:
                    station_stop(station);
                    return;
                case ZeroNetEventType.CenterStationRemove:
                    station_uninstall(station);
                    return;
                case ZeroNetEventType.CenterStationDocument:
                    station_document(station, content);
                    return;
                case ZeroNetEventType.CenterClientJoin:
                    client_join(station, content);
                    return;
                case ZeroNetEventType.CenterClientLeft:
                    client_left(station, content);
                    return;
            }
        }

        private static void station_uninstall(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_uninstall", name);
            config.State = ZeroCenterState.Remove;
            ZeroApplication.Config.Remove(config);
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationRemove, null, config);
        }

        private static void station_install(string name, string content)
        {
            if (!ZeroApplication.Config.UpdateConfig(name, content, out var config))
                return;
            ZeroTrace.SystemLog("station_install", name, content);
            config.State = ZeroCenterState.None;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationInstall, content, config);
        }

        private static void station_update(string name, string content)
        {
            if (!ZeroApplication.Config.UpdateConfig(name, content, out var config))
                return;
            ZeroTrace.SystemLog("station_update", name, content);
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationUpdate, content, config);
        }

        private static void station_closing(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_closing", name);
            if (config.State < ZeroCenterState.Closing)
                config.State = ZeroCenterState.Closing;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationClosing, null, config);
        }

        private static void station_resume(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_resume", name);
            config.State = ZeroCenterState.Run;
            if (ZeroApplication.InRun)
                ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationResume, null, config);
        }

        private static void station_pause(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_pause", name);
            config.State = ZeroCenterState.Pause;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationPause, null, config);
        }

        private static void station_left(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_left", name);
            if (config.State < ZeroCenterState.Closed)
                config.State = ZeroCenterState.Closed;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationLeft, null, config);
        }

        private static void station_stop(string name)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroTrace.SystemLog("station_stop", name);
            if (config.State < ZeroCenterState.Stop)
                config.State = ZeroCenterState.Stop;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationLeft, null, config);
        }

        private static void station_join(string name, string content)
        {
            if (!ZeroApplication.Config.UpdateConfig(name, content, out var config))
                return;
            ZeroTrace.SystemLog("station_join", content);
            config.State = ZeroCenterState.Run;
            if (!ZeroApplication.InRun)
                return;
            ZeroApplication.OnStationStateChanged(config);
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationJoin, content, config);
        }

        /// <summary>
        /// 站点心跳
        /// </summary>
        /// <param name="name"></param>
        /// <param name="content"></param>
        private static void station_state(string name, string content)
        {
            //ZeroTrace.WriteInfo("station_state",name);
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            try
            {
                ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationState, content, config);
            }
            catch (Exception e)
            {
                LogRecorder.Exception(e);
                ZeroTrace.WriteException("station_state", e, name, content);
            }
        }


        private static void center_start(string content)
        {
            if (Interlocked.CompareExchange(ref ZeroApplication.AppState, StationState.Initialized, StationState.Failed) == StationState.Failed)
            {
                ZeroTrace.SystemLog("center_start", content);
                ZeroApplication.JoinCenter();
            }
        }

        private static void center_closing(string content)
        {
            if (Interlocked.CompareExchange(ref ZeroApplication.AppState, StationState.Closing, StationState.Run) == StationState.Run)
            {
                ZeroTrace.SystemLog("center_close", content);
                ZeroApplication.RaiseEvent(ZeroNetEventType.CenterSystemClosing);
                ZeroApplication.ZerCenterStatus = ZeroCenterState.Closed;
                ZeroApplication.OnZeroEnd();
                ZeroApplication.ApplicationState = StationState.Failed;
            }
        }
        private static void station_document(string station, string content)
        {
            var doc = JsonConvert.DeserializeObject<StationDocument>(content);
            if (ZeroApplication.Config.Documents.ContainsKey(station))
            {
                if (!ZeroApplication.Config.Documents[station].IsLocal)
                    ZeroApplication.Config.Documents[station] = doc;
            }
            else
            {
                ZeroApplication.Config.Documents.Add(station, doc);
            }
            if (!ZeroApplication.Config.TryGetConfig(station, out var config))
                return;
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterStationDocument, station, config);
        }

        private static void center_stop(string content)
        {
            if (ZeroApplication.ZerCenterStatus == ZeroCenterState.Destroy)
                return;
            ZeroApplication.ZerCenterStatus = ZeroCenterState.Destroy;
            ZeroTrace.SystemLog("center_stop ", content);
        }

        /// <summary>
        /// 站点心跳
        /// </summary>
        private static void worker_sound_off()
        {
            if (ZeroApplication.CanDo)
                ZeroApplication.OnHeartbeat();
        }


        private static void client_join(string name, string content)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterClientJoin, name, config);
        }

        private static void client_left(string name, string content)
        {
            if (!ZeroApplication.Config.TryGetConfig(name, out var config))
                return;
            ZeroApplication.InvokeEvent(ZeroNetEventType.CenterClientLeft, name, config);
        }
        #endregion
    }

}