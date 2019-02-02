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
        Random r;
        System.IO.StreamWriter sw;
        
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
#if !RAND
                label1.Text = "No Device";
                label2.Text = "";
#endif

                if (chk_count-- == 0)
                {
                    get_device();
                    chk_count = chk_interval;
                }

#if !RAND
            }
            else
            {
                var temp = d.Temperature;
                var humid = d.Humidity;
#else
                if (r == null)
                    r = new Random();
                var temp = r.NextDouble() * 100.0;
                var humid = r.NextDouble() * 50.0;
#endif

                if (double.IsNaN(temp) || double.IsNaN(humid))
                {
                    d = null;
                }
                else
                {
#if RAND
                    label1.Text = temp.ToString("F2") + " °C (R)";
                    label2.Text = humid.ToString("F2") + " % (R)";
#else
                    label1.Text = temp.ToString("F2") + " °C";
                    label2.Text = humid.ToString("F2") + " %";
#endif
                    if (sw != null)
                    {
                        var dt = System.DateTime.Now;
                        sw.WriteLine(dt.ToString("yyyyMMdd\tHH:mm:ss.fff") + "\t" +
                            temp.ToString("F2") + "\t" +
                            humid.ToString("F2"));
                        sw.Flush();
                    }
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

            var fname = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments) +
                "/temphumid-" +
                System.DateTime.Now.ToString("yyyyMMdd") + ".txt";
            try
            {
                var s = new System.IO.FileStream(fname, System.IO.FileMode.Append,
                    System.IO.FileAccess.Write, System.IO.FileShare.ReadWrite);
                sw = new System.IO.StreamWriter(s);

                this.Text = this.Text + " " + fname;
            }
            catch (Exception) { }

            update();

            timer1.Enabled = true;
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            update();
        }
    }
}
