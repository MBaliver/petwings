// Ghidra headless script: decompile all PetWings game functions, grouped by original source module.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class DumpGame extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File tsv = new File(args[0]);
        File outDir = new File(args[1]);
        outDir.mkdirs();
        Map<String, String> fn2mod = new HashMap<>();
        for (String line : java.nio.file.Files.readAllLines(tsv.toPath())) {
            String[] p = line.split("\t");
            if (p.length == 2) fn2mod.put(p[1].trim(), p[0].replace(".obj", "").trim());
        }
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        Map<String, PrintWriter> outs = new TreeMap<>();
        int n = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String full = f.getName(true);
            String mod = fn2mod.get(full);
            if (mod == null) continue;
            PrintWriter w = outs.computeIfAbsent(mod, m -> {
                try { return new PrintWriter(new File(outDir, m + ".c")); } catch (IOException e) { throw new RuntimeException(e); }
            });
            DecompileResults r = d.decompileFunction(f, 120, monitor);
            w.println("// ===== " + full + " @ " + f.getEntryPoint());
            if (r != null && r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
            else w.println("// DECOMPILE FAILED: " + (r == null ? "null" : r.getErrorMessage()));
            n++;
        }
        for (PrintWriter w : outs.values()) w.close();
        println("Decompiled " + n + " functions into " + outs.size() + " modules");
    }
}
