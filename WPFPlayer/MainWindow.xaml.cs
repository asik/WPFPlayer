using System.Windows;
using System.Windows.Forms.Integration;

namespace WPFPlayer
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        WPFOpenGLLib.OpenGLUserControl m_openGLUserControl;

        public MainWindow()
        {
            InitializeComponent();
            var winFormHost = FindName("WinFormHost") as WindowsFormsHost;
            m_openGLUserControl = new WPFOpenGLLib.OpenGLUserControl(); ;
            winFormHost.Child = m_openGLUserControl;

            //var m_pictureBox = FindName("m_pictureBox") as PictureBox;
            //m_nativeInterop = new NativeApisInterop.NativeApisInteropClass();
            //m_nativeInterop.Init(openglControl.Handle, openglControl.Width, openglControl.Height);

        }

        void Window_Loaded(object sender, RoutedEventArgs e) {
            var videoPath = "F:/RAZOR/Videos/Quality test60fps.avi.mp4";//"F:/RAZOR/Videos/32bit plus lag.avi";   
            m_openGLUserControl.StartPlayback(videoPath);
        }

        
    }
}
