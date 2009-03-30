/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
* 
* Created by: Henrik Sundberg / sthesu
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/

using System;
using System.Collections.Generic;
using System.Text;

namespace dose_test_dotnet
{
    class Executor :
        Safir.Dob.StopHandler,
        Safir.Dob.MessageSubscriber,
        Safir.Dob.EntityHandler,
        Safir.Dob.ServiceHandler
    {
        public Executor(string[] args)
        {
            m_instance = int.Parse(args[0]);
            m_instanceString = m_instance.ToString();
            m_partnerEntityId = new Safir.Dob.Typesystem.EntityId(DoseTest.Partner.ClassTypeId, 
                                                                  new Safir.Dob.Typesystem.InstanceId(m_instance));
            m_controlConnectionName = m_identifier + "_control";
            m_testConnectionName = "partner_test_connection";
            m_callbackActions = new Dictionary<Safir.Dob.CallbackId.Enumeration, List<DoseTest.Action>>();
            foreach (Safir.Dob.CallbackId.Enumeration cb in Enum.GetValues(typeof(Safir.Dob.CallbackId.Enumeration)))
            {
                m_callbackActions.Add(cb, new List<DoseTest.Action>());
            }

            m_controlDispatcher = new Dispatcher(m_controlDispatchEvent);
            m_testDispatcher = new Dispatcher(m_testDispatchEvent);
            m_controlConnection.Open(m_controlConnectionName, m_instanceString, 0, this, m_controlDispatcher);

            //subscribe to messages going to everyone and to me.
            m_controlConnection.SubscribeMessage(DoseTest.Action.ClassTypeId, new Safir.Dob.Typesystem.ChannelId(m_instance), this);
            m_controlConnection.SubscribeMessage(DoseTest.Action.ClassTypeId, new Safir.Dob.Typesystem.ChannelId(), this);
        }

        public void Run()
        {
            System.Console.WriteLine(m_identifier + ":" + m_instance + " Started");
            System.Threading.AutoResetEvent[] waitHandles = new System.Threading.AutoResetEvent[]
            {
                m_controlDispatchEvent,
                m_testDispatchEvent,
                m_stopEvent
            };

            while (!m_isDone)
            {
                // Wait for initiation to finish
                int which = System.Threading.WaitHandle.WaitAny(waitHandles, TimeSpan.FromMilliseconds(1000), false);
                switch (which)
                {
                    case 0:
                        try
                        {
                            m_controlConnection.Dispatch();
                        }
                        catch (Safir.Dob.Typesystem.Exception exc)
                        {
                            Logger.Instance.WriteLine("Caught Exception when Dispatching controlConnection: " + 
                                exc.GetType().Name);
                            System.Console.WriteLine("Exception info: " + exc);
                        }
                        catch (Safir.Dob.Typesystem.FundamentalException exc)
                        {
                            Logger.Instance.WriteLine("Caught FundamentalException when Dispatching controlConnection: " + 
                                exc.GetType().Name);
                            System.Console.WriteLine("Exception info: " + exc);
                        }

                        break;

                    case 1:
                        if (m_dispatchTestConnection && m_isActive)
                        {
                            try
                            {
                                ExecuteCallbackActions(Safir.Dob.CallbackId.Enumeration.OnDoDispatch);
                                foreach (Consumer consumer in m_consumers)
                                {
                                    consumer.ExecuteCallbackActions(Safir.Dob.CallbackId.Enumeration.OnDoDispatch);
                                }

                                m_testConnection.Dispatch();
                            }
                            catch (Safir.Dob.Typesystem.Exception exc)
                            {
                                Logger.Instance.WriteLine("Caught Exception when Dispatching testConnection: " + exc.GetType().Name);
                                System.Console.WriteLine("Exception info: " + exc);
                            }
                            catch (Safir.Dob.Typesystem.FundamentalException exc)
                            {
                                Logger.Instance.WriteLine("Caught FundamentalException when Dispatching testConnection: " + exc.GetType().Name);
                                System.Console.WriteLine("Exception info: " + exc);
                            }
                        }
                        break;
                    case 2:
                        Logger.Instance.WriteLine("Got stop order");
                        ExecuteCallbackActions(Safir.Dob.CallbackId.Enumeration.OnStopOrder);
                        m_isDone = true;
                        break;
                }
            }
        }

        private void ExecuteAction(DoseTest.Action action)
        {
            switch (action.ActionType.Val)
            {
                case DoseTest.ActionEnum.Enumeration.Activate:
                    if (action.Identifier == m_identifier)
                    {
                        System.Console.WriteLine("Activating");
                        if (!m_isActive)
                        {
                            m_controlConnection.RegisterEntityHandler(m_partnerEntityId.TypeId,
                                new Safir.Dob.Typesystem.HandlerId(m_instance),
                                Safir.Dob.InstanceIdPolicy.Enumeration.HandlerDecidesInstanceId,
                                this);
                            m_controlConnection.RegisterServiceHandler
                                (DoseTest.Dump.ClassTypeId,
                                 new Safir.Dob.Typesystem.HandlerId(m_instance), this);
                        }
                        DoseTest.Partner partner = new DoseTest.Partner();
                        partner.Incarnation.Val = 0;
                        partner.Identifier.Val = m_identifier;
                        m_controlConnection.SetAll(partner, m_partnerEntityId.InstanceId,
                            new Safir.Dob.Typesystem.HandlerId(m_instance));
                        m_isActive = true;
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Deactivate:
                    if (action.Identifier == m_identifier)
                    {
                        m_isActive = false;
                        System.Console.WriteLine("Deactivating");
                        m_testConnection.Close();
                        m_controlConnection.UnregisterHandler(m_partnerEntityId.TypeId, new Safir.Dob.Typesystem.HandlerId(m_instance));
                        m_controlConnection.UnregisterHandler(DoseTest.Dump.ClassTypeId,new Safir.Dob.Typesystem.HandlerId(m_instance));
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Reset:
                    if (m_isActive)
                    {
                        m_testConnection.Close();
                        System.GC.Collect();
                        m_testConnection.Open(m_testConnectionName, m_instanceString, 0, null, m_testDispatcher);
                        using (Safir.Dob.EntityProxy ep = m_controlConnection.Read(m_partnerEntityId))
                        {
                            DoseTest.Partner partner = ep.Entity as DoseTest.Partner;
                            partner.Incarnation.Val = partner.Incarnation.Val + 1;
                            m_controlConnection.SetChanges(partner, m_partnerEntityId.InstanceId, new Safir.Dob.Typesystem.HandlerId(m_instance));
                        }
                        Consumer[] oldCons = m_consumers; //keep these for a while, so we get new consumer addresses.
                        m_consumers = new Consumer[3];

                        for (int i = 0; i < 3; ++i)
                        {
                            m_consumers[i] = new Consumer(i, m_testConnectionName, m_instanceString);
                        }

                        if (oldCons != null)//avoid warning...
                        {
                            oldCons = null; //avoid warning, again...
                        }
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Open:
                    {
                        if (m_isActive)
                        {
                            m_testConnection.Open(m_testConnectionName, m_instanceString, 0, null, m_testDispatcher);
                        }
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Close:
                    {
                        if (m_isActive)
                        {
                            m_testConnection.Close();
                        }
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.InhibitDispatch:
                    if (m_isActive)
                    {
                        m_dispatchTestConnection = !action.Inhibit.Val;
                        Logger.Instance.WriteLine("InhibitDispatch set to " + m_dispatchTestConnection);
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Print:
                    if (m_isActive)
                    {
                        Logger.Instance.WriteLine(action.PrintString.Val);
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.ResetCallbackActions:
                    foreach (KeyValuePair<Safir.Dob.CallbackId.Enumeration, List<DoseTest.Action>> cbActions
                        in m_callbackActions)
                    {
                        cbActions.Value.Clear();
                    }
                    break;

                case DoseTest.ActionEnum.Enumeration.Sleep:
                    {
                        if (m_isActive)
                        {
                            System.Console.WriteLine("Sleeping " + action.SleepDuration.Val + " seconds");
                            System.Threading.Thread.Sleep((int)(action.SleepDuration.Val * 1000.0));
                        }
                    }
                    break;

                default:
                    Logger.Instance.WriteLine("Got unexpected action " + action.ActionType.Val);
                    break;
            }
        }

        void AddCallbackAction(DoseTest.Action action)
        {
            m_callbackActions[action.ActionCallback.Val].Add(action);
        }


        void ExecuteCallbackActions(Safir.Dob.CallbackId.Enumeration callback)
        {
            foreach (DoseTest.Action action in m_callbackActions[callback])
            {
                ExecuteAction(action);
            }
        }


        #region StopHandler Members

        void Safir.Dob.StopHandler.OnStopOrder()
        {
            m_stopEvent.Set();
        }

        #endregion

        #region MessageSubscriber Members

        void Safir.Dob.MessageSubscriber.OnMessage(Safir.Dob.MessageProxy messageProxy)
        {
            DoseTest.Action action = messageProxy.Message as DoseTest.Action;

            if (action.Consumer.IsNull())
            {//No consumer set, meant for the executor.
                if (action.ActionCallback.IsNull()) //it is a normal action
                {
                    ExecuteAction(action);
                }
                else if (m_isActive)
                {
                    AddCallbackAction(action);
                }
            }
            else if (m_isActive)
            {
                Consumer theConsumer = m_consumers[action.Consumer.Val];

                if (action.ActionCallback.IsNull()) //it is a normal action
                {
                    theConsumer.ExecuteAction(action);
                }
                else
                {
                    theConsumer.AddCallbackAction(action);
                }
            }
        }

        #endregion

        #region RevokedRegistrationBase Members

        void Safir.Dob.RevokedRegistrationBase.OnRevokedRegistration(long typeId, Safir.Dob.Typesystem.HandlerId handlerId)
        {
            if (m_isActive)
            {
                Logger.Instance.WriteLine("Deactivating");
                m_testConnection.Close();
                m_controlConnection.UnregisterHandler(m_partnerEntityId.TypeId, new Safir.Dob.Typesystem.HandlerId(m_instance));
                m_controlConnection.UnregisterHandler(DoseTest.Dump.ClassTypeId, new Safir.Dob.Typesystem.HandlerId(m_instance));
                m_isActive = false;
            }
        }

        #endregion

        #region EntityRequestBase Members

        void Safir.Dob.EntityRequestBase.OnCreateRequest(Safir.Dob.EntityRequestProxy entityRequestProxy, Safir.Dob.ResponseSender responseSender)
        {
            responseSender.Send(new Safir.Dob.ErrorResponse());
        }

        void Safir.Dob.EntityRequestBase.OnDeleteRequest(Safir.Dob.EntityRequestProxy entityRequestProxy, Safir.Dob.ResponseSender responseSender)
        {
            responseSender.Send(new Safir.Dob.ErrorResponse());
        }

        void Safir.Dob.EntityRequestBase.OnUpdateRequest(Safir.Dob.EntityRequestProxy entityRequestProxy, Safir.Dob.ResponseSender responseSender)
        {
            responseSender.Send(new Safir.Dob.ErrorResponse());
        }

        #endregion

        #region ServiceRequestBase Members

        void Safir.Dob.ServiceRequestBase.OnServiceRequest(Safir.Dob.ServiceRequestProxy serviceRequestProxy, Safir.Dob.ResponseSender responseSender)
        {
            DoseTest.DumpResult result = new DoseTest.DumpResult();
            result.Result.Val = Logger.Instance.ToString();
            Logger.Instance.Clear();
            responseSender.Send(result);
        }

        #endregion

        #region Dispatcher subclass
        private class Dispatcher : Safir.Dob.Dispatcher
        {
            public Dispatcher(System.Threading.AutoResetEvent theEvent)
            {
                m_event = theEvent;
            }
            #region Dispatcher Members

            void Safir.Dob.Dispatcher.OnDoDispatch()
            {
                m_event.Set();
            }

            #endregion

            private System.Threading.AutoResetEvent m_event;
        }
        #endregion

        #region Data members

        private readonly string m_identifier = "dotnet";
        private readonly int m_instance;
        private readonly string m_instanceString;
        private readonly string m_controlConnectionName;
        private readonly string m_testConnectionName;
        private readonly Safir.Dob.Typesystem.EntityId m_partnerEntityId;
        private bool m_isDone = false;
        private bool m_isActive = false;
        Consumer[] m_consumers;

        private Safir.Dob.Connection m_controlConnection = new Safir.Dob.Connection();
        private Safir.Dob.Connection m_testConnection = new Safir.Dob.Connection();
        private bool m_dispatchTestConnection = true;


        private System.Threading.AutoResetEvent m_controlDispatchEvent = new System.Threading.AutoResetEvent(false);
        private System.Threading.AutoResetEvent m_testDispatchEvent = new System.Threading.AutoResetEvent(false);
        private System.Threading.AutoResetEvent m_stopEvent = new System.Threading.AutoResetEvent(false);

        private Dispatcher m_controlDispatcher;
        private Dispatcher m_testDispatcher;
        Dictionary<Safir.Dob.CallbackId.Enumeration, List<DoseTest.Action>> m_callbackActions;
        #endregion




    }
}
