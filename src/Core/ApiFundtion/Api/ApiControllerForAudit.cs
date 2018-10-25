// // /*****************************************************
// // (c)2016-2016 Copy right www.gboxt.com
// // ����:
// // ����:Agebull.DataModel
// // ����:2016-06-12
// // �޸�:2016-06-16
// // *****************************************************/

#region ����

using System.Collections.Generic;

using Agebull.Common.DataModel;
using Agebull.Common.DataModel.BusinessLogic;
using Agebull.Common.Rpc;
using Agebull.ZeroNet.ZeroApi;
using Gboxt.Common.DataModel;
using Gboxt.Common.DataModel.Extends;
using Gboxt.Common.DataModel.MySql;

#endregion

namespace Agebull.Common.WebApi
{
    /// <summary>
    ///     ���֧��APIҳ��Ļ���
    /// </summary>
    public abstract class ApiControllerForAudit<TData, TAccess, TDatabase, TBusinessLogic> :
        ApiControllerForDataState<TData, TAccess, TDatabase, TBusinessLogic>
        where TData : EditDataObject, IStateData, IHistoryData, IAuditData, IIdentityData, new()
        where TAccess : DataStateTable<TData, TDatabase>, new()
        where TBusinessLogic : BusinessLogicByAudit<TData, TAccess, TDatabase>, new()
        where TDatabase : MySqlDataBase
    {
        /// <summary>
        ///     �ύ���
        /// </summary>
        protected virtual void OnSubmitAudit()
        {
            var ids = GetLongArrayArg("selects");
            if (!DoValidate(ids))
                return;
            if (!Business.Submit(ids))
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }

        /// <summary>
        ///     �˻����
        /// </summary>
        private void OnBackAudit()
        {
            var ids = GetLongArrayArg("selects");
            if (!Business.Back(ids))
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }

        /// <summary>
        ///     ���
        /// </summary>
        private void OnUnAudit()
        {
            var ids = GetLongArrayArg("selects");
            if (!Business.UnAudit(ids))
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }

        /// <summary>
        ///     ���
        /// </summary>
        protected virtual void OnAuditPass()
        {
            var ids = GetLongArrayArg("selects");
            if (!DoValidate(ids))
            {
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
                return;
            }
            var result = Business.AuditPass(ids);
            if (!result)
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }

        private bool DoValidate(IEnumerable<long> ids)
        {
            var message = new ValidateResultDictionary();
            var succeed = Business.Validate(ids, message.TryAdd);

            if (message.Result.Count > 0)
            {
                GlobalContext.Current.LastMessage = message.ToString();
            }
            return succeed;
        }

        /// <summary>
        ///     ����
        /// </summary>
        private void OnPullback()
        {
            var ids = GetLongArrayArg("selects");
            if (!Business.Pullback(ids))
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }

        /// <summary>
        ///     ���
        /// </summary>
        private void OnAuditDeny()
        {
            var ids = GetLongArrayArg("selects");
            if (!Business.AuditDeny(ids))
                GlobalContext.Current.LastState = ErrorCode.LogicalError;
        }


        /// <summary>
        ///     ȡ���б�����
        /// </summary>
        protected override ApiPageData<TData> GetListData(LambdaItem<TData> lambda)
        {
            var audit = GetIntArg("audit", -1);
            if (audit == 0x100 || audit < 0)
                return base.GetListData(lambda);
            if (audit <= (int) AuditStateType.End)
            {
                lambda.AddRoot(p => p.AuditState == (AuditStateType)audit);
                return base.GetListData(lambda);
            }

            switch (audit)
            {
                case 0x10: //����
                case 0xFF: //ɾ��
                    SetArg("dataState", audit);
                    break;
                case 0x13: //ͣ��
                    SetArg("dataState", (int)DataStateType.Disable);
                    break;
                case 0x11: //δ���
                    lambda.AddRoot(p => p.AuditState <= AuditStateType.Again);
                    break;
                case 0x12: //δ����
                    lambda.AddRoot(p => p.AuditState < AuditStateType.End);
                    break;
            }

            return base.GetListData(lambda);
        }

        #region API

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/deny")]
        public ApiResult AuditDeny()
        {
            
            OnAuditDeny();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/pullback")]
        public ApiResult Pullback()
        {
            
            OnPullback();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/submit")]
        public ApiResult SubmitAudit()
        {
            
            OnSubmitAudit();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/validate")]
        public ApiResult Validate()
        {
            
            DoValidate(GetLongArrayArg("selects"));
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }


        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/pass")]
        public ApiResult AuditPass()
        {
            
            OnAuditPass();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/redo")]
        public ApiResult UnAudit()
        {
            OnUnAudit();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        /// <summary>
        ///     ���ڽ����ϵĵ�ǰ�û����Է��ʵİ�ť����
        /// </summary>
        
        [Route("audit/back")]
        public ApiResult BackAudit()
        {
            
            OnBackAudit();
            return IsFailed
                ? (new ApiResult
                {
                    Success = false,
                    Status = GlobalContext.Current.LastStatus
                })
                : ApiResult.Ok;
        }

        #endregion
    }

    /// <summary>
    ///     ���֧��APIҳ��Ļ���
    /// </summary>
    public abstract class ApiControllerForAudit<TData, TAccess, TDatabase> :
        ApiControllerForDataState<TData, TAccess, TDatabase, BusinessLogicByAudit<TData, TAccess, TDatabase>>
        where TData : EditDataObject, IStateData, IHistoryData, IAuditData, IIdentityData, new()
        where TAccess : DataStateTable<TData, TDatabase>, new()
        where TDatabase : MySqlDataBase
    {
    }
}