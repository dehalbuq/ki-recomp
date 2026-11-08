package br.com.dehalbuq.kiandroid;

import android.content.pm.PackageManager;
import android.view.KeyEvent;
import java.io.File;
import org.libsdl.app.SDLActivity;

/** SDL2 arcade player: native touch interface + Android TV Bluetooth remote. */
public class KIActivity extends SDLActivity {
    @Override
    protected String[] getArguments() {
        final String data = new File(getFilesDir(), "kinst").getAbsolutePath();
        final boolean touch = getPackageManager().hasSystemFeature(
            PackageManager.FEATURE_TOUCHSCREEN);
        return touch
            ? new String[] {"--data", data, "--aspect", "4:3", "--touchscreen"}
            : new String[] {"--data", data, "--aspect", "4:3"};
    }

    // Prevent SDLActivity's default Back behavior from instantly finishing
    // the app. Send a native key event; the game shows a confirmation prompt
    // and exits only on a second Back within five seconds.
    @Override
    public void onBackPressed() {
        SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
        SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                if (event.getRepeatCount() == 0)
                    SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
}
