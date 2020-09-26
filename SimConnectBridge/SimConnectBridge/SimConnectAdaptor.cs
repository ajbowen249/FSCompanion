using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.FlightSimulator.SimConnect;
using System.Runtime.InteropServices;
using System.Windows.Interop;
using System.Windows;
using System.Runtime.Serialization;

namespace SimConnectBridge {
    public class SimConnectAdaptor {
        private enum Definitions {
            Throttle = 0x01,
            Mixture =  0x02,
            ElevatorTrim =  0x03,
            FlapsPositions = 0x04,
            FlapsIndex = 0x05,
        }

        struct DoubleProp {
            public double value;
        }

        struct IntProp {
            public int value;
        }

        [DataContract]
        public class SimProps {
            [DataMember]
            public double throttle;
            public double mixture;
            public double elevatorTrim;
            public int flapsPositions;
            public int flapsIndex;
        }

        [DataContract]
        public class SimPropsUpdate {
            [DataMember]
            public double? throttle;
            public double? mixture;
            public double? elevatorTrim;
            public int? flapsIndex;
        }

        private const int WM_USER_SIMCONNECT = 0x0402;
        private const uint SC_UserId = 0;

        private IntPtr _handle;
        HwndSource _handleSource;
        private SimConnect _simConnect;
        private SimProps _simProps = new SimProps();
        private object _rwLock = new object();

        public SimConnectAdaptor(Window window) {
            _handle = new WindowInteropHelper(window).Handle;
            _handleSource = HwndSource.FromHwnd(_handle);
            _handleSource.AddHook(HandleSimConnectEvents);

            TryConnect();
        }

        public SimProps GetSimProps() {
            lock (_rwLock) {
                return _simProps;
            }
        }

        public SimProps SetSimProps(SimPropsUpdate update) {
            lock (_rwLock) {
                if (update.throttle != null) {
                    _simProps.throttle = (double)update.throttle;
                    SetDoubleProp(Definitions.Throttle, _simProps.throttle);
                }

                if (update.mixture != null) {
                    _simProps.mixture = (double)update.mixture;
                    SetDoubleProp(Definitions.Mixture, _simProps.mixture);
                }

                if (update.elevatorTrim != null) {
                    _simProps.elevatorTrim = (double)update.elevatorTrim;
                    SetDoubleProp(Definitions.ElevatorTrim, _simProps.elevatorTrim);
                }

                if (update.flapsIndex != null) {
                    _simProps.flapsIndex = (int)update.flapsIndex;
                    SetIntProp(Definitions.FlapsIndex, _simProps.flapsIndex);
                }

                return _simProps;
            }
        }


        public void TryConnect() {
            if (_simConnect == null) {
                try {
                    _simConnect = new SimConnect("SimConnect Bridge", _handle, WM_USER_SIMCONNECT, null, 0);
                    ConfigureSimConnectData();
                } catch {
                    _simConnect = null;
                }
            }
        }

        public bool IsConnected {
            get { return _simConnect != null; }
        }

        private void ConfigureSimConnectData() {
            _simConnect.OnRecvSimobjectData += new SimConnect.RecvSimobjectDataEventHandler(OnReceiveSimObjectData);
            RegisterDoubleProp(Definitions.Throttle, "GENERAL ENG THROTTLE LEVER POSITION:1", "percent");
            RegisterDoubleProp(Definitions.Mixture, "GENERAL ENG MIXTURE LEVER POSITION:1", "percent");
            RegisterDoubleProp(Definitions.ElevatorTrim, "ELEVATOR TRIM POSITION", "radians");
            RegisterIntProp(Definitions.FlapsPositions, "FLAPS NUM HANDLE POSITIONS", "number");
            RegisterIntProp(Definitions.FlapsIndex, "FLAPS HANDLE INDEX", "number");
        }

        private void OnDisconnected() {
            _simConnect = null;
        }

        private void OnReceiveSimObjectData(SimConnect sender, SIMCONNECT_RECV_SIMOBJECT_DATA data) {
            lock (_rwLock) {
                switch((Definitions)data.dwRequestID) {
                    case Definitions.Throttle:
                        _simProps.throttle = ReadDoubleProp(data);
                        break;
                    case Definitions.Mixture:
                        _simProps.mixture = ReadDoubleProp(data);
                        break;
                    case Definitions.ElevatorTrim:
                        _simProps.elevatorTrim = ReadDoubleProp(data);
                        break;
                    case Definitions.FlapsPositions:
                        _simProps.flapsPositions = ReadIntProp(data);
                        break;
                    case Definitions.FlapsIndex:
                        _simProps.flapsIndex = ReadIntProp(data);
                        break;
                    default:
                        break;
                }
            }
        }

        ~SimConnectAdaptor() {
            if (_handleSource != null) {
                _handleSource.RemoveHook(HandleSimConnectEvents);
            }
        }

        private IntPtr HandleSimConnectEvents(IntPtr hWnd, int message, IntPtr wParam, IntPtr lParam, ref bool isHandled) {
            isHandled = false;

            switch (message) {
                case WM_USER_SIMCONNECT: {
                        if (_simConnect != null) {
                            try {
                                _simConnect.ReceiveMessage();
                                isHandled = true;
                            } catch {
                                OnDisconnected();
                            }
                        }
                    }
                    break;

                default:
                    break;
            }

            return IntPtr.Zero;
        }

        private void SetDoubleProp(Definitions field, double value) {
            if (IsConnected) {
                DoubleProp prop;
                prop.value = value;
                _simConnect.SetDataOnSimObject(field, (uint)SIMCONNECT_SIMOBJECT_TYPE.USER, SIMCONNECT_DATA_SET_FLAG.DEFAULT, prop);
            }
        }

        private void SetIntProp(Definitions field, int value) {
            if (IsConnected) {
                IntProp prop;
                prop.value = value;
                _simConnect.SetDataOnSimObject(field, (uint)SIMCONNECT_SIMOBJECT_TYPE.USER, SIMCONNECT_DATA_SET_FLAG.DEFAULT, prop);
            }
        }

        private double ReadDoubleProp(SIMCONNECT_RECV_SIMOBJECT_DATA data) {
            return ((DoubleProp)data.dwData[0]).value;
        }

        private int ReadIntProp(SIMCONNECT_RECV_SIMOBJECT_DATA data) {
            return ((IntProp)data.dwData[0]).value;
        }

        private void RegisterDoubleProp(Definitions field, string name, string unit) {
            _simConnect.AddToDataDefinition(field, name, unit, SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
            _simConnect.RegisterDataDefineStruct<DoubleProp>(field);
            _simConnect.RequestDataOnSimObject(field, field, SC_UserId, SIMCONNECT_PERIOD.SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG.CHANGED, 0, 0, 0);
        }

        private void RegisterIntProp(Definitions field, string name, string unit) {
            _simConnect.AddToDataDefinition(field, name, unit, SIMCONNECT_DATATYPE.INT32, 0.0f, SimConnect.SIMCONNECT_UNUSED);
            _simConnect.RegisterDataDefineStruct<IntProp>(field);
            _simConnect.RequestDataOnSimObject(field, field, SC_UserId, SIMCONNECT_PERIOD.SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG.CHANGED, 0, 0, 0);
        }
    }
}
