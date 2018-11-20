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

        private void update()
        {
            if(d == null)
            {
                label1.Text = "No Device";
                label2.Text = "";
            }
            else
            {
                label1.Text = d.Temperature.ToString("F2") + " °C";
                label2.Text = d.Humidity.ToString("F2") + " %";
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            var l = new libtemphumidnet.LibTempHumid();
            var ds = l.Devices;
            if (ds.Count > 0)
            {
                d = ds[0];
                timer1.Enabled = true;
            }

            update();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            update();
        }
    }
}
