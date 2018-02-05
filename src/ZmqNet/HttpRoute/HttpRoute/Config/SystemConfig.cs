﻿using System;
using System.IO;
using System.Runtime.Serialization;
using Agebull.Common.Logging;
using Newtonsoft.Json;

namespace ZeroNet.Http.Route
{
    /// <summary>
    /// 系统配置
    /// </summary>
    [JsonObject(MemberSerialization.OptIn), DataContract, Serializable]
    internal class SystemConfig
    {
        /// <summary>
        /// 是否加入ZeroNet
        /// </summary>
        [JsonProperty]
        internal bool FireZero { get; set; }
        /// <summary>
        /// 当前服务器Key
        /// </summary>
        [JsonProperty]
        internal string ServiceKey { get; set; }

        /// <summary>
        /// 黑洞地址
        /// </summary>
        [JsonProperty]
        internal string BlockHost { get; set; }
        /// <summary>
        /// 超时时间
        /// </summary>
        [JsonProperty]
        internal int HttpTimeOut { get; set; }
        /// <summary>
        /// 触发警告的执行时间
        /// </summary>
        [JsonProperty]
        internal int WaringTime { get; set; }

        /// <summary>
        /// 内容页地址
        /// </summary>
        [JsonProperty]
        internal string ContextHost { get; set; }

        /// <summary>
        /// 记录跟踪日志
        /// </summary>
        [JsonProperty]
        internal bool LogMonitor { get; set; }

        /// <summary>
        /// 记录跟踪日志
        /// </summary>
        [JsonProperty]
        internal string LogPath { get => TxtRecorder.LogPath; set => TxtRecorder.LogPath = value; }

        /// <summary>
        /// 是否检查Auth头
        /// </summary>
        [JsonProperty]
        internal bool CheckBearerCache { get; set; }
    }
}