/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//--------------------------------------------------------------------------------
// Include files
//--------------------------------------------------------------------------------
#include <jni.h>
#include <errno.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "TeapotRenderer.h"
#include "NDKHelper.h"
#include "JavaUI.h"

//-------------------------------------------------------------------------
//Preprocessor
//-------------------------------------------------------------------------
#define HELPER_CLASS_NAME "com.sample.helper.NDKHelper" //Class name of helper function
#define JUIHELPER_CLASS_NAME "com.sample.helper.JUIHelper" //Class name of JUIhelper function
#define HELPER_CLASS_SONAME "JavaUINativeActivity" //Share object name of helper function library
//-------------------------------------------------------------------------
//Engine class of the sample
//-------------------------------------------------------------------------
struct android_app;
class Engine
{
    TeapotRenderer renderer_;

    ndk_helper::GLContext* gl_context_;
    ndk_helper::SensorManager sensroManager_;

    bool initialized_resources_;
    bool has_focus_;

    ndk_helper::DoubletapDetector doubletap_detector_;
    ndk_helper::PinchDetector pinch_detector_;
    ndk_helper::DragDetector drag_detector_;
    ndk_helper::PerfMonitor monitor_;

    ndk_helper::TapCamera tap_camera_;

    jui_helper::JUITextView* textViewFPS_;

    android_app* app_;
    jui_helper::JUIDialog* dialog_;

    void InitUI();

    void TransformPosition( ndk_helper::Vec2& vec );

    void UpdateFPS( float fFPS );

    void ShowDialog();
    void ShowAlertDialog();

public:
    static void HandleCmd( struct android_app* app,
            int32_t cmd );
    static int32_t HandleInput( android_app* app,
            AInputEvent* event );

    Engine();
    ~Engine();
    void SetState( android_app* state );
    int InitDisplay( const int32_t cmd );
    void LoadResources();
    void UnloadResources();
    void DrawFrame();
    void TermDisplay( const int32_t cmd );
    void TrimMemory();
    bool IsReady();

    void UpdatePosition( AInputEvent* event,
            int32_t iIndex,
            float& fX,
            float& fY );

    //Sensor managers
    void ProcessSensors( const int32_t id )
    {
        sensroManager_.Process( id );
    }
    void ResumeSensors()
    {
        sensroManager_.Resume();
    }
    void SuspendSensors()
    {
        sensroManager_.Suspend();
    }
};

//-------------------------------------------------------------------------
//Ctor
//-------------------------------------------------------------------------
Engine::Engine() :
        initialized_resources_( false ), has_focus_( false ), app_( NULL ), dialog_( NULL ), textViewFPS_( NULL )
{
    gl_context_ = ndk_helper::GLContext::GetInstance();
}

//-------------------------------------------------------------------------
//Dtor
//-------------------------------------------------------------------------
Engine::~Engine()
{
    jui_helper::JUIWindow::GetInstance()->Close();
    delete dialog_;
}

/**
 * Load resources
 */
void Engine::LoadResources()
{
    renderer_.Init();
    renderer_.Bind( &tap_camera_ );
}

/**
 * Unload resources
 */
void Engine::UnloadResources()
{
    renderer_.Unload();
}

/**
 * Initialize an EGL context for the current display.
 */
int Engine::InitDisplay( const int32_t cmd )
{
    if( !initialized_resources_ )
    {
        gl_context_->Init( app_->window );
        InitUI();
        LoadResources();
        initialized_resources_ = true;
    }
    else
    {
        // initialize OpenGL ES and EGL
        if( EGL_SUCCESS != gl_context_->Resume( app_->window ) )
        {
            UnloadResources();
            LoadResources();
        }

        jui_helper::JUIWindow::GetInstance()->Resume( app_->activity, cmd );
    }

    // Initialize GL state.
    glEnable (GL_CULL_FACE);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);

    //Note that screen size might have been changed
    glViewport( 0, 0, gl_context_->GetScreenWidth(), gl_context_->GetScreenHeight() );
    renderer_.UpdateViewport();

    tap_camera_.SetFlip( 1.f, -1.f, -1.f );
    tap_camera_.SetPinchTransformFactor( 2.f, 2.f, 8.f );

    return 0;
}

/*
 * Initialize java UI,
 * invoking jui_helper functions to create java UIs
 */
void Engine::InitUI()
{
    //Show toast with app label
    ndk_helper::JNIHelper::GetInstance()->RunOnUiThread( []()
    {
        jui_helper::JUIToast toast(ndk_helper::JNIHelper::GetInstance()->GetAppLabel());
        toast.Show();
    } );

    // The window is being shown, get it ready.
    jui_helper::JUIWindow::Init( app_->activity, JUIHELPER_CLASS_NAME );

    //
    //Setup SeekBar
    //
    jui_helper::JUISeekBar* seekBar = new jui_helper::JUISeekBar();
    seekBar->SetCallback( jui_helper::JUICALLBACK_SEEKBAR_START_TRACKING_TOUCH,
            [](jui_helper::JUIView* view, const int32_t mes, const int32_t p1, const int32_t p2 )
            {
                LOGI( "Seek start" );
            } );
    seekBar->SetCallback( jui_helper::JUICALLBACK_SEEKBAR_STOP_TRACKING_TOUCH,
            [](jui_helper::JUIView* view, const int32_t mes, const int32_t p1, const int32_t p2 )
            {
                LOGI( "Seek stop" );
            } );
    seekBar->SetCallback( jui_helper::JUICALLBACK_SEEKBAR_PROGRESSCHANGED,
            [](jui_helper::JUIView* view, const int32_t mes, const int32_t p1, const int32_t p2 )
            {
                LOGI( "Seek progress %d", p1 );
            } );

    //Configure relative layout parameter
    seekBar->AddRule( jui_helper::LAYOUT_PARAMETER_ALIGN_PARENT_BOTTOM, jui_helper::LAYOUT_PARAMETER_TRUE );
    seekBar->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    seekBar->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_MATCH_PARENT, jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT );
    seekBar->SetMargins( 10, 10, 10, 100 );
    jui_helper::JUIWindow::GetInstance()->AddView( seekBar );

    //
    //Setup Label
    //
    jui_helper::JUITextView* textView = new jui_helper::JUITextView( "TextView" );
    textView->SetAttribute( "Gravity", jui_helper::ATTRIBUTE_GRAVITY_CENTER );
    textView->SetAttribute( "TextColor", 0xffffffff );
    textView->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, seekBar );
    textView->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    textView->SetMargins( 0, 50, 0, 50 );
    jui_helper::JUIWindow::GetInstance()->AddView( textView );

    //
    //CheckBox
    //
    jui_helper::JUICheckBox* checkBox = new jui_helper::JUICheckBox( "CheckBox" );
    checkBox->SetCallback( [](jui_helper::JUIView* view, bool b )
    {
        LOGI( "CheckBox %d", b );
    } );
    checkBox->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT, jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT, 0.5f );

    //
    //Switch
    //
    jui_helper::JUISwitch* sw = new jui_helper::JUISwitch( "Switch" );
    sw->SetCallback( [](jui_helper::JUIView* view, bool b )
    {
        LOGI( "Switch %d", b );
    } );
    sw->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT, jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT, 0.5f );
    sw->SetMargins( 50, 0, 0, 0 );

    //
    //Linear layout
    //
    jui_helper::JUILinearLayout* layout = new jui_helper::JUILinearLayout();
    layout->AddView( sw );
    layout->AddView( checkBox );
    layout->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, textView );
    layout->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_MATCH_PARENT, jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT );
    layout->SetMargins( 0, 50, 0, 50 );
    jui_helper::JUIWindow::GetInstance()->AddView( layout );

    //
    //RadioButton
    //
    jui_helper::JUIRadioButton* radio1 = new jui_helper::JUIRadioButton( "Radio1" );
    radio1->SetCallback( [](jui_helper::JUIView* view, bool b )
    {
        LOGI( "Radio1 %d", b );
    } );
    radio1->SetAttribute( "Checked", true );
    jui_helper::JUIRadioButton* radio2 = new jui_helper::JUIRadioButton( "Radio2" );
    radio2->SetCallback( [](jui_helper::JUIView* view, bool b )
    {
        LOGI( "Radio2 %d", b );
    } );

    //
    //RadioGroup layout
    //
    jui_helper::JUIRadioGroup* radioGroup = new jui_helper::JUIRadioGroup();
    radioGroup->AddView( radio1 );
    radioGroup->AddView( radio2 );
    radioGroup->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, layout );
    radioGroup->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    radioGroup->SetAttribute( "Orientation", jui_helper::LAYOUT_ORIENTATION_HORIZONTAL );
    radioGroup->SetMargins( 0, 50, 0, 50 );
    radioGroup->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT, jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT );

    jui_helper::JUIWindow::GetInstance()->AddView( radioGroup );

    //
    //Button
    //
    jui_helper::JUIButton* button = new jui_helper::JUIButton( "Dialog" );
    button->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, radioGroup );
    button->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    button->SetCallback( [this](jui_helper::JUIView* view, const int32_t message)
    {
        LOGI( "Button click: %d", message );
        switch( message )
        {
            case jui_helper::JUICALLBACK_BUTTON_UP:
            {
                ShowDialog();
            }
            break;
            default:
            break;
        }
    } );
    button->SetMargins( 0, 50, 0, 50 );
    jui_helper::JUIWindow::GetInstance()->AddView( button );

    jui_helper::JUIButton* button2 = new jui_helper::JUIButton( "Alert" );
    button2->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, radioGroup );
    button2->AddRule( jui_helper::LAYOUT_PARAMETER_LEFT_OF, button );
    button2->SetMargins( 0, 50, 0, 50 );
    button2->SetCallback( [this](jui_helper::JUIView* view, const int32_t message)
    {
        LOGI( "Button click: %d", message );
        switch( message )
        {
            case jui_helper::JUICALLBACK_BUTTON_UP:
            {
                ShowAlertDialog();
            }
            break;
            default:
            break;
        }
    } );
    jui_helper::JUIWindow::GetInstance()->AddView( button2 );

    //
    //Toggle Button
    //
    jui_helper::JUIToggleButton* toggle = new jui_helper::JUIToggleButton( "ON", "OFF" );

    toggle->AddRule( jui_helper::LAYOUT_PARAMETER_ABOVE, radioGroup );
    toggle->AddRule( jui_helper::LAYOUT_PARAMETER_RIGHT_OF, button );
    toggle->SetMargins( 0, 50, 0, 50 );
    jui_helper::JUIWindow::GetInstance()->AddView( toggle );

    textViewFPS_ = new jui_helper::JUITextView( "0.0FPS" );
    textViewFPS_->SetAttribute( "Gravity", jui_helper::ATTRIBUTE_GRAVITY_LEFT );
    textViewFPS_->SetAttribute( "TextColor", 0xffffffff );
    textViewFPS_->SetAttribute( "TextSize", jui_helper::ATTRIBUTE_UNIT_SP, 18.f );
    textViewFPS_->SetAttribute( "Padding", 10, 10, 10, 10 );
    jui_helper::JUIWindow::GetInstance()->AddView( textViewFPS_ );

    return;
}

/*
 * Show dialog through jui_helper
 */
void Engine::ShowDialog()
{
    if( dialog_ )
        delete dialog_;

    dialog_ = new jui_helper::JUIDialog( app_->activity );

    /*
     * Progress bar
     */
    jui_helper::JUIProgressBar* progressBarHorizontal = new jui_helper::JUIProgressBar(
            jui_helper::PROGRESS_BAR_STYLE_HIROZONTAL );
    progressBarHorizontal->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    progressBarHorizontal->AddRule( jui_helper::LAYOUT_PARAMETER_ALIGN_PARENT_TOP, jui_helper::LAYOUT_PARAMETER_TRUE );
    progressBarHorizontal->SetLayoutParams( jui_helper::ATTRIBUTE_SIZE_MATCH_PARENT,
            jui_helper::ATTRIBUTE_SIZE_WRAP_CONTENT );
    progressBarHorizontal->SetAttribute( "Indeterminate", true );

    /*
     * Progress bar (circle)
     */
    jui_helper::JUIProgressBar* progressBar = new jui_helper::JUIProgressBar();
    progressBar->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    progressBar->AddRule( jui_helper::LAYOUT_PARAMETER_BELOW, progressBarHorizontal );

    /*
     * OK button
     */
    jui_helper::JUIButton* button = new jui_helper::JUIButton( "OK" );
    button->AddRule( jui_helper::LAYOUT_PARAMETER_CENTER_IN_PARENT, jui_helper::LAYOUT_PARAMETER_TRUE );
    button->AddRule( jui_helper::LAYOUT_PARAMETER_BELOW, progressBar );
    button->SetCallback( [this](jui_helper::JUIView* view, const int32_t message)
    {
        switch( message )
        {
            case jui_helper::JUICALLBACK_BUTTON_UP:
            {
                dialog_->Close();
            }
        }
    } );

    dialog_->SetCallback( jui_helper::JUICALLBACK_DIALOG_CANCELLED,
            [this](jui_helper::JUIDialog* dialog, const int32_t message )
            {
                LOGI("Dialog cancelled");
            } );
    dialog_->SetCallback( jui_helper::JUICALLBACK_DIALOG_DISMISSED,
            [this](jui_helper::JUIDialog* dialog, const int32_t message )
            {
                LOGI("Dialog dismissed");
            } );
    dialog_->SetAttribute( "Title", "Dialog" );
    dialog_->AddView( progressBarHorizontal );
    dialog_->AddView( progressBar );
    dialog_->AddView( button );
    dialog_->Show();
}

/*
 * Show alert dialog through jui_helper
 */
void Engine::ShowAlertDialog()
{
    if( dialog_ )
        delete dialog_;

    jui_helper::JUIAlertDialog* dialog = new jui_helper::JUIAlertDialog( app_->activity );

    dialog->SetCallback( jui_helper::JUICALLBACK_DIALOG_CANCELLED,
            [this](jui_helper::JUIDialog* dialog, const int32_t message )
            {
                LOGI("Dialog cancelled");
            } );
    dialog->SetCallback( jui_helper::JUICALLBACK_DIALOG_DISMISSED,
            [this](jui_helper::JUIDialog* dialog, const int32_t message )
            {
                LOGI("Dialog dismissed");
            } );
    dialog->SetAttribute( "Title", "Alert" );
    dialog->SetAttribute( "Message", "AlertMessage" );

    /*
     * Button
     */
    dialog->SetButton( jui_helper::ALERTDIALOG_BUTTON_POSITIVE, "Yes",
            [](jui_helper::JUIView* view, const int32_t message)
            {
                LOGI("Pressed Positive Button");
            } );
    dialog->SetButton( jui_helper::ALERTDIALOG_BUTTON_NEUTRAL, "Maybe",
            [](jui_helper::JUIView* view, const int32_t message)
            {
                LOGI("Pressed Neutral Button");
            } );
    dialog->SetButton( jui_helper::ALERTDIALOG_BUTTON_NEGATIVE, "No",
            [](jui_helper::JUIView* view, const int32_t message)
            {
                LOGI("Pressed Negative Button");
            } );

    dialog->Show();
    dialog_ = dialog;
}

/**
 * Just the current frame in the display.
 */
void Engine::DrawFrame()
{
    float fFPS;
    if( monitor_.Update( fFPS ) )
    {
        UpdateFPS( fFPS );
    }
    renderer_.Update( monitor_.GetCurrentTime() );

    // Just fill the screen with a color.
    glClearColor( 0.5f, 0.5f, 0.5f, 1.f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    renderer_.Render();

    // Swap
    if( EGL_SUCCESS != gl_context_->Swap() )
    {
        UnloadResources();
        LoadResources();
    }
}

/**
 * Tear down the EGL context currently associated with the display.
 */
void Engine::TermDisplay( const int32_t cmd )
{
    gl_context_->Suspend();
    jui_helper::JUIWindow::GetInstance()->Suspend( cmd );
}

void Engine::TrimMemory()
{
    LOGI( "Trimming memory" );
    gl_context_->Invalidate();
}

/**
 * Process the next input event.
 */
int32_t Engine::HandleInput( android_app* app,
        AInputEvent* event )
{
    Engine* eng = (Engine*) app->userData;
    if( AInputEvent_getType( event ) == AINPUT_EVENT_TYPE_MOTION )
    {
        ndk_helper::GESTURE_STATE doubleTapState = eng->doubletap_detector_.Detect( event );
        ndk_helper::GESTURE_STATE dragState = eng->drag_detector_.Detect( event );
        ndk_helper::GESTURE_STATE pinchState = eng->pinch_detector_.Detect( event );

        //Double tap detector has a priority over other detectors
        if( doubleTapState == ndk_helper::GESTURE_STATE_ACTION )
        {
            //Detect double tap
            eng->tap_camera_.Reset( true );
        }
        else
        {
            //Handle drag state
            if( dragState & ndk_helper::GESTURE_STATE_START )
            {
                //Otherwise, start dragging
                ndk_helper::Vec2 v;
                eng->drag_detector_.GetPointer( v );
                eng->TransformPosition( v );
                eng->tap_camera_.BeginDrag( v );
            }
            else if( dragState & ndk_helper::GESTURE_STATE_MOVE )
            {
                ndk_helper::Vec2 v;
                eng->drag_detector_.GetPointer( v );
                eng->TransformPosition( v );
                eng->tap_camera_.Drag( v );
            }
            else if( dragState & ndk_helper::GESTURE_STATE_END )
            {
                eng->tap_camera_.EndDrag();
            }

            //Handle pinch state
            if( pinchState & ndk_helper::GESTURE_STATE_START )
            {
                //Start new pinch
                ndk_helper::Vec2 v1;
                ndk_helper::Vec2 v2;
                eng->pinch_detector_.GetPointers( v1, v2 );
                eng->TransformPosition( v1 );
                eng->TransformPosition( v2 );
                eng->tap_camera_.BeginPinch( v1, v2 );
            }
            else if( pinchState & ndk_helper::GESTURE_STATE_MOVE )
            {
                //Multi touch
                //Start new pinch
                ndk_helper::Vec2 v1;
                ndk_helper::Vec2 v2;
                eng->pinch_detector_.GetPointers( v1, v2 );
                eng->TransformPosition( v1 );
                eng->TransformPosition( v2 );
                eng->tap_camera_.Pinch( v1, v2 );
            }
        }
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
void Engine::HandleCmd( struct android_app* app,
        int32_t cmd )
{
    Engine* eng = (Engine*) app->userData;
    LOGI( "message %d", cmd );
    switch( cmd )
    {
    case APP_CMD_SAVE_STATE:
        break;
    case APP_CMD_INIT_WINDOW:
        if( app->window != NULL )
        {
            eng->InitDisplay( APP_CMD_INIT_WINDOW );
            eng->DrawFrame();
        }

        break;
    case APP_CMD_TERM_WINDOW:
        //Note that JUI helper needs to know if a window has been terminated
        eng->TermDisplay( APP_CMD_TERM_WINDOW );

        eng->has_focus_ = false;
        break;
    case APP_CMD_START:
        break;
    case APP_CMD_STOP:
        break;
    case APP_CMD_RESUME:
        jui_helper::JUIWindow::GetInstance()->Resume( app->activity, APP_CMD_RESUME );
        break;
    case APP_CMD_GAINED_FOCUS:
        //Start animation
        eng->ResumeSensors();
        eng->has_focus_ = true;
        jui_helper::JUIWindow::GetInstance()->Resume( app->activity, APP_CMD_GAINED_FOCUS );
        break;
    case APP_CMD_LOST_FOCUS:
        // Also stop animating.
        eng->SuspendSensors();
        eng->has_focus_ = false;
        eng->DrawFrame();
        break;
    case APP_CMD_LOW_MEMORY:
        //Free up GL resources
        eng->TrimMemory();
        break;
    case APP_CMD_CONFIG_CHANGED:
        //Configuration changes
        eng->TermDisplay( APP_CMD_CONFIG_CHANGED );
        eng->InitDisplay( APP_CMD_CONFIG_CHANGED );
        break;
    }
}

//-------------------------------------------------------------------------
//Misc
//-------------------------------------------------------------------------
void Engine::SetState( android_app* state )
{
    app_ = state;
    doubletap_detector_.SetConfiguration( app_->config );
    drag_detector_.SetConfiguration( app_->config );
    pinch_detector_.SetConfiguration( app_->config );
    sensroManager_.Init( state );
}

bool Engine::IsReady()
{
    if( has_focus_ )
        return true;

    return false;
}

void Engine::TransformPosition( ndk_helper::Vec2& vec )
{
    vec = ndk_helper::Vec2( 2.0f, 2.0f ) * vec
            / ndk_helper::Vec2( gl_context_->GetScreenWidth(), gl_context_->GetScreenHeight() )
            - ndk_helper::Vec2( 1.f, 1.f );
}

void Engine::UpdateFPS( float fFPS )
{
    ndk_helper::JNIHelper::GetInstance()->RunOnUiThread( [fFPS, this]()
    {
        const int32_t count = 64;
        char str[count];
        snprintf( str, count, "%2.2f FPS", fFPS );
        textViewFPS_->SetAttribute( "Text", (const char*)str );
    } );

    return;
}

Engine g_engine;

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main( android_app* state )
{
    app_dummy();

    g_engine.SetState( state );

    //Init helper functions
    ndk_helper::JNIHelper::Init( state->activity, HELPER_CLASS_NAME, HELPER_CLASS_SONAME );

    state->userData = &g_engine;
    state->onAppCmd = Engine::HandleCmd;
    state->onInputEvent = Engine::HandleInput;

    // loop waiting for stuff to do.
    while( 1 )
    {
        // Read all pending events.
        int id;
        int events;
        android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while( (id = ALooper_pollAll( g_engine.IsReady() ? 0 : -1, NULL, &events, (void**) &source )) >= 0 )
        {
            // Process this event.
            if( source != NULL )
                source->process( state, source );

            g_engine.ProcessSensors( id );

            // Check if we are exiting.
            if( state->destroyRequested != 0 )
            {
                g_engine.TermDisplay( APP_CMD_TERM_WINDOW );
                return;
            }
        }

        if( g_engine.IsReady() )
        {
            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            g_engine.DrawFrame();
        }
    }
}

extern "C"
{
JNIEXPORT
void Java_com_sample_javaui_JavaUINativeActivity_OnPauseHandler( JNIEnv* env )
{
    //This call is to suppress 'E/WindowManager(): android.view.WindowLeaked...' errors.
    //Since orientation change events in NativeActivity comes later than expected, we can not dismiss
    //popupWindow gracefully from NativeActivity.
    //So we are releasing popupWindows explicitly triggered from Java callback through JNI call.
    ndk_helper::JNIHelper::GetInstance()->Lock( false );
    jui_helper::JUIWindow::GetInstance()->Suspend( APP_CMD_PAUSE );
    ndk_helper::JNIHelper::GetInstance()->Unlock();
}
}
;

