package io.johnsonlee.graffito

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.FileObserver
import android.os.IBinder
import android.util.Log
import android.webkit.MimeTypeMap
import androidx.core.content.FileProvider
import java.io.File

class TraceService : Service() {

    private companion object {
        const val TAG = "TraceService"
    }

    private val binder: IBinder by lazy {
        TraceServiceBinder()
    }

    private val observer: TraceObserver by lazy(this@TraceService::TraceObserver)

    override fun onCreate() {
        super.onCreate()
        observer.startWatching()
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        observer.stopWatching()
        super.onDestroy()
    }

    @Suppress("DEPRECATION")
    private inner class TraceObserver : FileObserver(filesDir.canonicalPath) {

        override fun onEvent(event: Int, path: String?) {
            Log.d(TAG, "$path: $event")
            val name = path ?: return

            if (CLOSE_WRITE == event && name.startsWith("trace-") && name.endsWith(".txt")) {
                val trace = File(filesDir, name)
                val uri = FileProvider.getUriForFile(this@TraceService, "$packageName.file", trace)
                val mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(trace.extension)
                startActivity(Intent(Intent.ACTION_VIEW).apply {
                    setDataAndType(uri, mimeType)
                    addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                })
            }
        }

    }

    internal class TraceServiceBinder : Binder()

}