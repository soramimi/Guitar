using System;

public class runme {
  static void Main(String[] argv) {
    try {
      Console.WriteLine (MeCab.MeCab.VERSION);
      String arg = "";
      for (int i = 0; i < argv.Length; ++i) {
	arg += " ";
	arg += argv[i];
      }
      MeCab.Tagger t = new MeCab.Tagger (arg);
      String s = "太郎は花子が持っている本を次郎に渡した。";
      Console.WriteLine (t.parse(s));

      for (MeCab.Node n = t.parseToNode(s); n != null ; n = n.next) {
	Console.WriteLine (n.surface + "\t" + n.feature);
      }

      MeCab.Model model = new MeCab.Model(arg);
      MeCab.Tagger t2 = model.createTagger();
      Console.WriteLine (t2.parse(s));

      MeCab.Lattice lattice = model.createLattice();
      Console.WriteLine(s);
      lattice.set_sentence(s);
      if (t2.parse(lattice)) {
        Console.WriteLine(lattice.toString());
      }

      lattice.add_request_type(MeCab.MeCab.MECAB_NBEST);
      lattice.set_sentence(s);
      t2.parse(lattice);
      for (int i = 0; i < 10; ++i) {
        lattice.next();
        Console.WriteLine(lattice.toString());
      }
     }

     catch (Exception e) {
	Console.WriteLine("Generic Exception Handler: {0}", e.ToString());
     }
  }
}
