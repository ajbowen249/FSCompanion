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
        private bool _shown = false;

        public MainWindow() {
            InitializeComponent();
        }

        protected override void OnContentRendered(EventArgs args) {
            base.OnContentRendered(args);
            if (!_shown) {
                _adaptor = new SimConnectAdaptor(this);
                _server = new BridgeServer(_adaptor);
                _pollTimer = new DispatcherTimer();
                _pollTimer.Tick += (s, e) => {
                    throttleOutput.Text = _adaptor.GetSimProps().throttle.ToString();
                };

                _pollTimer.Interval = new TimeSpan(0, 0, 0, 0, 10);
                // _pollTimer.Start();

                _shown = true;
            }
        }
    }
}
