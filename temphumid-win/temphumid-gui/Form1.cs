using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace temphumid_gui
{
    public partial class Form1 : Form
    {
        libtemphumidnet.Device d;
        
        public Form1()
        {
            InitializeComponent();
        }

        int chk_count = 0;
        const int chk_interval = 4;

        private void update()
        {
            if(d == null)
            {
                label1.Text = "No Device";
                label2.Text = "";

                if (chk_count-- == 0)
                {
                    get_device();
                    chk_count = chk_interval;
                }
            }
            else
            {
                var temp = d.Temperature;
                var humid = d.Humidity;

                if (double.IsNaN(temp) || double.IsNaN(humid))
                {
                    d = null;
                }
                else
                {
                    label1.Text = d.Temperature.ToString("F2") + " °C";
                    label2.Text = d.Humidity.ToString("F2") + " %";
                }
            }
        }

        void get_device()
        {
            var l = new libtemphumidnet.LibTempHumid();
            var ds = l.Devices;
            if (ds.Count > 0)
            {
                d = ds[0];
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            get_device();

            update();

            timer1.Enabled = true;
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            update();
        }
    }
}
