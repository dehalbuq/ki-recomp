package br.com.dehalbuq.kiandroid;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.os.Bundle;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;

/** Copy private APK assets to app-private storage before entering SDL. */
public class BootstrapActivity extends Activity {
    private TextView status;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        status = new TextView(this);
        status.setTextSize(18);
        status.setPadding(32, 48, 32, 32);
        status.setText("Preparando Killer Instinct...");
        setContentView(status);
        new Thread(new Runnable() {
            @Override public void run() {
                try {
                    File destination = new File(getFilesDir(), "kinst");
                    if (!destination.isDirectory() && !destination.mkdirs()) {
                        throw new IOException("Cannot create private data directory");
                    }
                    extract("kinst/kinst.zip", new File(destination, "kinst.zip"));
                    extract("kinst/kinst.chd", new File(destination, "kinst.chd"));
                    runOnUiThread(new Runnable() {
                        @Override public void run() {
                            startActivity(new Intent(BootstrapActivity.this, KIActivity.class));
                            finish();
                        }
                    });
                } catch (final Exception e) {
                    runOnUiThread(new Runnable() {
                        @Override public void run() {
                            new AlertDialog.Builder(BootstrapActivity.this)
                                .setTitle("Erro ao preparar os arquivos")
                                .setMessage(e.toString())
                                .setPositiveButton("OK", null)
                                .show();
                        }
                    });
                }
            }
        }, "ki-assets-setup").start();
    }

    private void showProgress(final String message) {
        runOnUiThread(new Runnable() {
            @Override public void run() { status.setText(message); }
        });
    }

    private void extract(String asset, File dest) throws IOException {
        showProgress("Preparando " + dest.getName() + " na primeira inicializacao...");
        long expected;
        try (AssetFileDescriptor afd = getAssets().openFd(asset)) {
            expected = afd.getLength();
        }
        if (dest.isFile() && dest.length() == expected) return;
        File part = new File(dest.getAbsolutePath() + ".part");
        if (part.exists() && !part.delete()) throw new IOException("Could not remove prior partial asset");
        try (InputStream in = getAssets().open(asset); FileOutputStream out = new FileOutputStream(part)) {
            byte[] buffer = new byte[128 * 1024];
            int count;
            while ((count = in.read(buffer)) >= 0) {
                if (count > 0) out.write(buffer, 0, count);
            }
            out.getFD().sync();
        }
        if (part.length() != expected) throw new IOException("Incomplete copy: " + asset);
        if (dest.exists() && !dest.delete()) throw new IOException("Cannot replace old asset " + dest);
        if (!part.renameTo(dest)) throw new IOException("Cannot commit " + dest.getName());
    }
}
