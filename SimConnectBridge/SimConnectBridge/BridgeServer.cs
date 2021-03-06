﻿using System.Net;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading.Tasks;

namespace SimConnectBridge {
    class BridgeServer {
        SimConnectAdaptor _adaptor;
        HttpListener _server;
        Task _serverTask;
        DataContractJsonSerializer _simPropsSerializer = new DataContractJsonSerializer(typeof(SimConnectAdaptor.SimProps));
        DataContractJsonSerializer _uUpdateSerializer = new DataContractJsonSerializer(typeof(SimConnectAdaptor.SimPropsUpdate));

        public BridgeServer(SimConnectAdaptor adaptor) {
            _adaptor = adaptor;
            _server = new HttpListener();
            _server.Prefixes.Add($"http://+:8080/");
            _server.Start();

            _serverTask = Task.Run(() => RunServer());
        }

        private void RunServer() {
            while (true) {
                var context = _server.GetContext();
                var path = context.Request.Url.LocalPath;
                
                if (path == "/sim_props") {
                    if (context.Request.HttpMethod == "GET") {
                        getAllSimProps(context);
                    } else if (context.Request.HttpMethod == "POST") {
                        setSimProps(context);
                    }
                }

                string responseString = $"<HTML><BODY>No path for \"{path}\"</BODY></HTML>";
                byte[] buffer = Encoding.UTF8.GetBytes(responseString);
                var output = context.Response.OutputStream;
                context.Response.StatusCode = 404;
                output.Write(buffer, 0, buffer.Length);
                output.Close();
            }
        }

        private void getAllSimProps(HttpListenerContext context) {
            var simProps = _adaptor.GetSimProps();
            var output = context.Response.OutputStream;
            _simPropsSerializer.WriteObject(output, simProps);
            output.Close();
        }

        private void setSimProps(HttpListenerContext context) {
            var update = (SimConnectAdaptor.SimPropsUpdate)_uUpdateSerializer.ReadObject(context.Request.InputStream);
            var output = context.Response.OutputStream;
            if (update != null) {
                var props = _adaptor.SetSimProps(update);
                _simPropsSerializer.WriteObject(output, props);
            } else {
                _simPropsSerializer.WriteObject(output, _adaptor.GetSimProps());
            }
            output.Close();
        }
    }
}
