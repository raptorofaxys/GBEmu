using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace GBEmu
{
    public partial class MainForm : Form
    {
        GameBoy m_gb;

        public MainForm()
        {
            InitializeComponent();

            m_gb = new GameBoy("Tetris (JUE) (V1.1) [!].gb");

            Text = m_gb.GetRomName();
        }
    }
}
