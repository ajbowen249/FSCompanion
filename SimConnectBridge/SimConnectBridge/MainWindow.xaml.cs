using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace SimConnectBridge
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window {
        private SimConnectAdaptor _adaptor;
        private BridgeServer _server;
        private DispatcherTimer _pollTimer;
        private bool _isInitialized = false;

        public MainWindow() {
            InitializeComponent();
        }

        protected override void OnContentRendered(EventArgs args) {
            base.OnContentRendered(args);
            if (!_isInitialized) {
                Initialize();
            }
        }

        private void Initialize() {
            _adaptor = new SimConnectAdaptor(this);
            _server = new BridgeServer(_adaptor);

            ReconnectButton.Click += (s, e) => {
                _adaptor.TryConnect();
            };

            ThrottleSlider.ValueChanged += (s, e) => {
                var update = new SimConnectAdaptor.SimPropsUpdate();
                update.throttle = ThrottleSlider.Value;
                _adaptor.SetSimProps(update);
            };

            MixtureSlider.ValueChanged += (s, e) => {
                var update = new SimConnectAdaptor.SimPropsUpdate();
                update.mixture = MixtureSlider.Value;
                _adaptor.SetSimProps(update);
            };

            ElevatorTrimSlider.ValueChanged += (s, e) => {
                var update = new SimConnectAdaptor.SimPropsUpdate();
                update.elevatorTrim = ElevatorTrimSlider.Value;
                _adaptor.SetSimProps(update);
            };

            DecFlapsButton.Click += (s, e) => {
                var props = _adaptor.GetSimProps();
                var update = new SimConnectAdaptor.SimPropsUpdate();
                update.flapsIndex = props.flapsIndex - 1;
                _adaptor.SetSimProps(update);
            };

            IncFlapsButton.Click += (s, e) => {
                var props = _adaptor.GetSimProps();
                var update = new SimConnectAdaptor.SimPropsUpdate();
                update.flapsIndex = props.flapsIndex + 1;
                _adaptor.SetSimProps(update);
            };

            _pollTimer = new DispatcherTimer();
            _pollTimer.Tick += (s, e) => {
                ConnectedStatus.Text = _adaptor.IsConnected ? "Connected" : "Disconnected";
                ConnectedStatus.Background = _adaptor.IsConnected ? Brushes.Green : Brushes.Yellow;
                ReconnectButton.IsEnabled = !_adaptor.IsConnected;

                var simProps = _adaptor.GetSimProps();

                ThrottleSlider.Value = simProps.throttle;
                MixtureSlider.Value = simProps.mixture;
                ElevatorTrimSlider.Value = simProps.elevatorTrim;
                FlapsDisplay.Text = $"({simProps.flapsIndex}/{simProps.flapsPositions})";
            };

            _pollTimer.Interval = new TimeSpan(0, 0, 0, 0, 100);
            _pollTimer.Start();

            _isInitialized = true;
        }
    }
}
