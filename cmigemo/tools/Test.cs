// vim:set ts=4 sts=4 sw=4 tw=0:

using System;
using KaoriYa.Migemo;

public class Test
{
	public static int Main(string[] args)
	{
		Migemo m = new Migemo("../dict/migemo-dict");
		string res = m.Query("kaki");
		Console.WriteLine("kaki->"+res);
		return 0;
	}
}
