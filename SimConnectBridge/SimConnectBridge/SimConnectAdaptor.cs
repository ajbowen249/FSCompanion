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
            Throttle = 0x01
        }

        struct ThrottleControl {
            public double throttlePercent;
        }

        [DataContract]
        public class SimProps {
            [DataMember]
            public double throttle;
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

            _simConnect = new SimConnect("SimConnect Bridge", _handle, WM_USER_SIMCONNECT, null, 0);
            ConfigureSimConnectData();
        }

        public SimProps GetSimProps() {
            lock (_rwLock) {
                return _simProps;
            }
        }

        private void ConfigureSimConnectData() {
            _simConnect.OnRecvSimobjectData += new SimConnect.RecvSimobjectDataEventHandler(onReceiveSimObjectData);
            _simConnect.AddToDataDefinition(Definitions.Throttle, "GENERAL ENG THROTTLE LEVER POSITION:1", "percent", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
            _simConnect.RegisterDataDefineStruct<ThrottleControl>(Definitions.Throttle);
            _simConnect.RequestDataOnSimObject(Definitions.Throttle, Definitions.Throttle, SC_UserId, SIMCONNECT_PERIOD.SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG.DEFAULT, 0, 0, 0);
        }

        private void onReceiveSimObjectData(SimConnect sender, SIMCONNECT_RECV_SIMOBJECT_DATA data) {
            lock (_rwLock) {
                switch((Definitions)data.dwRequestID) {
                    case Definitions.Throttle:
                        var tc = (ThrottleControl)data.dwData[0];
                        _simProps.throttle = tc.throttlePercent;
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
                        _simConnect.ReceiveMessage();
                        isHandled = true;
                    }
                    break;

                default:
                    break;
            }

            return IntPtr.Zero;
        }
    }
}
