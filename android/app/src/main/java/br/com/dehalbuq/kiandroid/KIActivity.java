package br.com.dehalbuq.kiandroid;

import java.io.File;
import org.libsdl.app.SDLActivity;

public class KIActivity extends SDLActivity {
    @Override
    protected String[] getArguments() {
        return new String[] {
            "--data", new File(getFilesDir(), "kinst").getAbsolutePath(),
            "--aspect", "4:3"
        };
    }
}
