using DivaImGuiDotNet.Properties;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace DivaImGuiDotNet
{
    public struct Shaders
    {
        public int GLuint;
        public int GLenum;
        public string filename;
        public Shaders(int gLuint, int gLenum, string file)
        {
            GLuint = gLuint;
            GLenum = gLenum;
            filename = file;
        }
    }

    public struct StrReplace
    {
        public string before;
        public string after;
        public string filename;
        public string configurables_str;
        public string configurables_str2;
        public string configurables_str3;
        public string configurables;

        public StrReplace(string before, string after, string filename, string configurables = null, string configurables_str = null, string configurables_str2 = null, string configurables_str3 = null)
        {
            this.before = before;
            this.after = after;
            this.filename = filename;
            this.configurables_str = configurables_str;
            this.configurables_str2 = configurables_str2;
            this.configurables_str3 = configurables_str3;
            this.configurables = configurables;
        }
    }


    public static unsafe class Main
    {
        public static bool ShdInitialized = false;
        public static List<string> gpushortname = new List<string>();
        public static List<StrReplace> strReplaces = new List<StrReplace>();
        public static List<Shaders> shd = new List<Shaders>();
        [DllExport("Initialize")]
        public static int Initialize(int pdversion)
        {
            if (!Directory.Exists("shadersaft"))
                return 0;

            Console.WriteLine("[DivaImGui] Initializing DotNet");

            //var cdef = new WebClient();
            var letext = Encoding.ASCII.GetString(ZipTools.Unzip(Resources.shd));
            var strarr = letext.Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
            foreach (var i in strarr)
            {
                var c = i.Split(new[] { "," }, StringSplitOptions.None);
                shd.Add(new Shaders(int.Parse(c[0]), int.Parse(c[1]), c[2]));
            }

            try
            {
                string gamepath = Path.GetFullPath(Path.Combine(System.Reflection.Assembly.GetExecutingAssembly().Location, @"..\..\"));


                //if (!Directory.Exists(gamepath + "mdata\\M999"))
                //    Directory.CreateDirectory(gamepath + "mdata\\M999");

                //if (!Directory.Exists(gamepath + "mdata\\M999\\rom"))
                //    Directory.CreateDirectory(gamepath + "mdata\\M999\\rom");

                //if (!File.Exists(gamepath + "mdata\\M999\\info.txt"))
                //    File.WriteAllText(gamepath + "mdata\\M999\\info.txt", "#mdata_info\ndepend.length=0\nversion=20161030\n");

                //if (!File.Exists(gamepath + "mdata\\M999\\rom\\shader.farc"))
                {
                    /*
                    var farc = new MikuMikuLibrary.Archives.Farc.FarcArchive();
                    farc.Load(gamepath + "mdata\\M999\\rom\\shader.farc.bak");

                    foreach (var i in farc.Entries)
                    {
                        var thefile = farc.Open(i, MikuMikuLibrary.Archives.EntryStreamMode.MemoryStream);
                        var streamreader = new StreamReader(thefile);
                        string strmessage;
                        strmessage = streamreader.ReadToEnd();
                        var splitted = strmessage.Split(new string[] { "\n" }, StringSplitOptions.None);
                        splitted[0] = splitted[0] + "\n" + "#" + i;
                        strmessage = String.Join("\n", splitted);
                        //strmessage = strmessage + "\n" + "# " + i;
                        byte[] byteArray = Encoding.Default.GetBytes(strmessage);
                        MemoryStream stream = new MemoryStream(byteArray);
                        farc.Add(i, stream, false, ConflictPolicy.Replace);
                    }
                    farc.Save(gamepath + "mdata\\M999\\rom\\shader.farc");
                    */
                    /*
                    File.WriteAllBytes("_shaderaftstd.exe", Resources.shaderaftstd);

                    Process shd = new Process();
                    shd.StartInfo.FileName = "_shaderaftstd.exe";
                    shd.StartInfo.UseShellExecute = false;
                    shd.StartInfo.RedirectStandardError = true;
                    shd.StartInfo.RedirectStandardInput = true;
                    shd.StartInfo.RedirectStandardOutput = true;
                    shd.StartInfo.Arguments = System.Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(gamepath));
                    shd.Start();
                    shd.WaitForExit();
                    File.Delete("_shaderaftstd.exe");
                    */
                }

                try
                {
                    var gpus = NvAPIWrapper.GPU.PhysicalGPU.GetPhysicalGPUs();
                    foreach (var i in gpus)
                    {
                        Console.WriteLine("[DivaImGui] " + i.ArchitectInformation.ShortName);
                        gpushortname.Add(i.ArchitectInformation.ShortName.ToUpper());
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine("[DivaImGui]" + " NvApi Error " + e.Message);
                }

                //cdef.DownloadFile("https://notabug.org/lybxlpsv/apdshd/raw/master/shd.txt?t=" + (int)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds, Path.GetTempPath() + "lybshd.txt");

                if (pdversion == 1)
                {
                    var cdef = new WebClient();
                    cdef.DownloadFile("https://notabug.org/lybxlpsv/apdshd/raw/master/shd.txt?t=" + (int)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds, Path.GetTempPath() + "lybshd.txt");
                }
                else if (pdversion == 0)
                {
                    Process cmd = new Process();
                    cmd.StartInfo.FileName = "rundll32.exe";
                    cmd.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                    cmd.StartInfo.Arguments = System.Reflection.Assembly.GetExecutingAssembly().Location + ",DownloadShader";
                    //cmd.StartInfo.Arguments = "/C certutil.exe -urlcache -split -f \"https://notabug.org/lybxlpsv/apdshd/raw/master/shd.txt?t=\" %temp%\\lybshd.txt";
                    cmd.Start();
                    cmd.WaitForExit();
                }
                var lines = File.ReadAllLines(Path.GetTempPath() + "lybshd.txt");
                foreach (var i in lines)
                {
                    if (i.StartsWith("#"))
                    {
                        if (i.Remove(0, 1).Count() != 0)
                            Console.WriteLine("[DivaImGui] " + i.Remove(0, 1));
                    }
                    else
                    {
                        string[] split1 = i.Split(new string[] { "|!|" }, StringSplitOptions.None);
                        string[] split2 = split1[0].Split('|');
                        bool patch = false;
                        string fname = "";
                        int linec = -1;
                        foreach (var c in split2)
                        {
                            if (c.StartsWith("!"))
                            {
                                fname = c.Split('@')[0].Remove(0, 1) + ".txt";
                                if (c.Split('@').Length != 1)
                                    linec = int.Parse(c.Split('@')[1]);
                                if (File.Exists(gamepath + "shadersaft\\" + fname))
                                    patch = true;
                                Console.WriteLine("[DivaImGui] " + gamepath + "shadersaft\\" + fname + "|" + patch.ToString());
                            }

                            foreach (var g in gpushortname)
                            {
                                var culture = new CultureInfo("en-us");
                                if (culture.CompareInfo.IndexOf(g, c, CompareOptions.IgnoreCase) >= 0)
                                    patch = true;
                            }
                            // if (gpushortname.Any(d => d.Contains(c)))
                            //    patch = true;
                        }
                        if (patch)
                        {
                            string before = split1[1];
                            string after = split1[2];
                            string regex = split1[3];
                            if (linec != -1)
                                if (fname != "")
                                {
                                    try
                                    {
                                        var cstr = File.ReadAllLines(fname);
                                        after = after.Replace("<line>", cstr[linec]);
                                    }
                                    catch
                                    {
                                        after = after.Replace("<line>", split1[4]);
                                    }
                                }
                            strReplaces.Add(new StrReplace(before, after, regex));
                        }
                    }
                }

                ShdInitialized = true;
                Console.WriteLine("[DivaImGui] Shader Patch Initialized!");
            }
            catch (Exception Ex)
            {
                Console.WriteLine("[DivaImGui] Download failed!");
                Console.WriteLine(Ex.ToString());
            }

            return 1;
        }

        [DllExport("RefreshShaders")]
        public static void RefreshShader()
        {
            strReplaces.Clear();
            string gamepath = Path.GetFullPath(Path.Combine(System.Reflection.Assembly.GetExecutingAssembly().Location, @"..\..\"));
            var lines = File.ReadAllLines(Path.GetTempPath() + "lybshd.txt");
            foreach (var i in lines)
            {
                if (i.StartsWith("#"))
                    Console.WriteLine("[DivaImGui] " + i.Remove(0, 1));
                else
                {
                    string[] split1 = i.Split(new string[] { "|!|" }, StringSplitOptions.None);
                    string[] split2 = split1[0].Split('|');
                    bool patch = false;
                    string fname = "";
                    int linec = -1;
                    foreach (var c in split2)
                    {
                        if (c.StartsWith("!"))
                        {
                            fname = c.Split('@')[0].Remove(0, 1) + ".txt";
                            if (c.Split('@').Length != 1)
                                linec = int.Parse(c.Split('@')[1]);
                            if (File.Exists(gamepath + "shadersaft\\" + fname))
                                patch = true;
                        }

                        foreach (var g in gpushortname)
                        {
                            var culture = new CultureInfo("en-us");
                            if (culture.CompareInfo.IndexOf(g, c, CompareOptions.IgnoreCase) >= 0)
                                patch = true;
                        }
                        // if (gpushortname.Any(d => d.Contains(c)))
                        //    patch = true;
                    }
                    if (patch)
                    {
                        string before = split1[1];
                        string after = split1[2];
                        string regex = split1[3];
                        if (linec != -1)
                            if (fname != "")
                            {
                                try
                                {
                                    var cstr = File.ReadAllLines(fname);
                                    after = after.Replace("<line>", cstr[linec]);
                                }
                                catch
                                {
                                    after = after.Replace("<line>", split1[4]);
                                }
                            }
                        strReplaces.Add(new StrReplace(before, after, regex));
                    }
                }
            }
        }
        [DllExport("DownloadShader")]
        public static void DownloadShader()
        {
            var cdef = new WebClient();
            cdef.DownloadFile("https://notabug.org/lybxlpsv/apdshd/raw/master/shd.txt?t=" + (int)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds, Path.GetTempPath() + "lybshd.txt");
        }
        [DllExport("ProcessShader")]
        public static int ModifyShader(IntPtr ptr, IntPtr ptrtgt, int len, int glformat, int glid)
        {
            if (ShdInitialized)
            {
                var str = Marshal.PtrToStringAnsi(ptr, len);
                var strarr = str.Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
                var filename = "";
                try
                {
                    var shdobj = shd.Where(c => c.GLuint == glid).Where(c => c.GLenum == glformat).FirstOrDefault();
                    filename = "#" + shdobj.filename;
                    //Console.WriteLine(filename);
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.Message);
                    return -1;
                }
                //var filename = strarr[1];
                //Console.WriteLine("[DivaImGui] " + filename);
                if (filename.StartsWith("#"))
                {
                    foreach (var i in strReplaces)
                    {
                        System.Text.RegularExpressions.Regex regEx = new System.Text.RegularExpressions.Regex(i.filename);
                        if (regEx.IsMatch(filename.Remove(0, 1)))
                        {
                            str = str.Replace(i.before, i.after);
                            var count = str.Split(new string[] { i.before }, StringSplitOptions.None).Length - 1;
                            //if (count > 0) Console.WriteLine("[DivaImGui] Patched " + filename);
                        }
                    }
                    byte[] byteArray = Encoding.ASCII.GetBytes(str);
                    //File.AppendText(glid.ToString() + "," + glformat.ToString() + "," + filename).WriteLine();
                    Marshal.Copy(byteArray, 0, ptrtgt, byteArray.Length);
                    return byteArray.Length;
                }
                else return -1;
            }
            else return -1;
        }
    }
}
