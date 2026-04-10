import org.chasen.mecab.MeCab;
import org.chasen.mecab.Tagger;
import org.chasen.mecab.Model;
import org.chasen.mecab.Lattice;
import org.chasen.mecab.Node;

public class test {
  static {
    try {
       System.loadLibrary("MeCab");
    } catch (UnsatisfiedLinkError e) {
       System.err.println("Cannot load the example native code.\nMake sure your LD_LIBRARY_PATH contains \'.\'\n" + e);
       System.exit(1);
    }
  }

  public static void main(String[] argv) {
     System.out.println(MeCab.VERSION);
     Tagger tagger = new Tagger();
     String str = "太郎は二郎にこの本を渡した。";
     System.out.println(tagger.parse(str));
     Node node = tagger.parseToNode(str);
     for (;node != null; node = node.getNext()) {
	System.out.println(node.getSurface() + "\t" + node.getFeature());
     }
     System.out.println ("EOS\n");

     Model model = new Model();
     Tagger tagger2 = model.createTagger();
     System.out.println (tagger2.parse(str));

     Lattice lattice = model.createLattice();
     System.out.println(str);
     lattice.set_sentence(str);
     if (tagger2.parse(lattice)) {
       System.out.println(lattice.toString());
       for (node = lattice.bos_node(); node != null; node = node.getNext()) {
	  System.out.println(node.getSurface() + "\t" + node.getFeature());
       }
       System.out.println("EOS\n");
     }

     lattice.add_request_type(MeCab.MECAB_NBEST);
     lattice.set_sentence(str);
     tagger2.parse(lattice);
     for (int i = 0; i < 10; ++i) {
       if (lattice.next()) {
         System.out.println("nbest:" + i + "\n" +
                            lattice.toString());
       }
     }
  }
}
