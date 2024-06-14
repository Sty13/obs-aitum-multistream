#include "multistream.hpp"
#include "obs-module.h"
#include "version.h"
#include <obs-frontend-api.h>
#include <QDesktopServices>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <util/config-file.h>
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Aitum");
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-multistream", "en-US")

static MultistreamDock *multistream_dock;

bool obs_module_load(void)
{
	//return true;
	blog(LOG_INFO, "[Aitum-Multistream] loaded version %s", PROJECT_VERSION);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	multistream_dock = new MultistreamDock(main_window);
	obs_frontend_add_dock_by_id("AitumMultistreamDock", obs_module_text("AitumMultistream"), multistream_dock);

	return true;
}

void obs_module_unload()
{
	if (multistream_dock) {
		delete multistream_dock;
	}
}

void RemoveWidget(QWidget *widget);

void RemoveLayoutItem(QLayoutItem *item)
{
	if (!item)
		return;
	RemoveWidget(item->widget());
	if (item->layout()) {
		while (QLayoutItem *item2 = item->layout()->takeAt(0))
			RemoveLayoutItem(item2);
	}
	delete item;
}

void RemoveWidget(QWidget *widget)
{
	if (!widget)
		return;
	if (widget->layout()) {
		while (QLayoutItem *item = widget->layout()->takeAt(0)) {
			RemoveLayoutItem(item);
		}
		delete widget->layout();
	}
	delete widget;
}

MultistreamDock::MultistreamDock(QWidget *parent) : QFrame(parent)
{
	auto l = new QVBoxLayout;
	setLayout(l);
	auto t = new QWidget;
	auto tl = new QVBoxLayout;
	t->setLayout(tl);

	auto mainCanvasGroup = new QGroupBox(QString::fromUtf8(obs_module_text("MainCanvas")));
	//mainCanvasGroup->setObjectName("mainCanvasGroup");

	//mainCanvasGroup->setStyleSheet(QString("QGroupBox#mainCanvasGroup{background-color: %1;}") // padding-top: 4px;					       .arg(main_window->palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));

	mainCanvasLayout = new QVBoxLayout;

	auto mainStreamGroup = new QGroupBox;
	mainStreamGroup->setStyleSheet(QString("QGroupBox{background-color: %1; padding-top: 4px;}")
					       .arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));
	//mainStreamGroup->setStyleSheet(QString("QGroupBox{padding-top: 4px;}"));
	auto mainStreamLayout = new QVBoxLayout;

	auto l2 = new QHBoxLayout;
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinStream"))), 1);
	mainStreamButton = new QPushButton;
	mainStreamButton->setMinimumHeight(30);
	mainStreamButton->setObjectName(QStringLiteral("canvasStream"));
	mainStreamButton->setIcon(streamInactiveIcon);
	mainStreamButton->setCheckable(true);
	mainStreamButton->setChecked(false);
	connect(mainStreamButton, &QPushButton::clicked, [this] {
		if (obs_frontend_streaming_active()) {
			obs_frontend_streaming_stop();
			mainStreamButton->setChecked(false);
		} else {
			obs_frontend_streaming_start();
			mainStreamButton->setChecked(true);
		}
		mainStreamButton->setStyleSheet(
			QString::fromUtf8(mainStreamButton->isChecked() ? "background: rgb(0,210,153);" : ""));
		mainStreamButton->setIcon(mainStreamButton->isChecked() ? streamActiveIcon : streamInactiveIcon);
	});
	//streamButton->setSizePolicy(sp2);
	mainStreamButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	l2->addWidget(mainStreamButton);
	mainStreamLayout->addLayout(l2);

	mainStreamGroup->setLayout(mainStreamLayout);

	mainCanvasLayout->addWidget(mainStreamGroup);

	mainCanvasGroup->setLayout(mainCanvasLayout);

	tl->addWidget(mainCanvasGroup);
	tl->addStretch(1);

	//auto verticalCanvasGroup = new QGroupBox(QString::fromUtf8(obs_module_text("VerticalCanvas")));

	//tl->addWidget(verticalCanvasGroup);
	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(t);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	l->addWidget(scrollArea, 1);

	auto buttonRow = new QHBoxLayout;
	buttonRow->setContentsMargins(0, 0, 0, 0);
	auto configButton = new QPushButton;
	configButton->setMinimumHeight(30);
	configButton->setProperty("themeID", "configIconSmall");
	configButton->setFlat(true);
	configButton->setAutoDefault(false);
	//configButton->setSizePolicy(sp2);
	configButton->setToolTip(QString::fromUtf8(obs_module_text("AitumMultistreamSettings")));
	QPushButton::connect(configButton, &QPushButton::clicked, [this] {
		if (!configDialog)
			configDialog = new OBSBasicSettings((QMainWindow *)obs_frontend_get_main_window());

		auto settings = obs_data_create();
		if (current_config)
			obs_data_apply(settings, current_config);
		configDialog->LoadSettings(settings);
		configDialog->setResult(QDialog::Rejected);
		if (configDialog->exec() == QDialog::Accepted) {
			if (current_config) {
				obs_data_apply(current_config, settings);
				obs_data_release(settings);
				SaveSettings();
				LoadSettings();
			} else {
				current_config = settings;
			}
		} else {
			obs_data_release(settings);
		}
	});

	buttonRow->addWidget(configButton);

	auto aitumButton = new QPushButton;
	aitumButton->setMinimumHeight(30);
	//aitumButton->setSizePolicy(sp2);
	aitumButton->setIcon(QIcon(":/aitum/media/aitum.png"));
	aitumButton->setToolTip(QString::fromUtf8("https://aitum.tv"));
	QPushButton::connect(aitumButton, &QPushButton::clicked, [] { QDesktopServices::openUrl(QUrl("https://aitum.tv")); });
	buttonRow->addWidget(aitumButton);

	l->addLayout(buttonRow);

	obs_frontend_add_event_callback(frontend_event, this);
}

MultistreamDock::~MultistreamDock()
{
	obs_data_release(current_config);
	obs_frontend_remove_event_callback(frontend_event, this);
	multistream_dock = nullptr;
}

void MultistreamDock::frontend_event(enum obs_frontend_event event, void *private_data)
{
	auto md = (MultistreamDock *)private_data;
	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED || event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		md->LoadSettingsFile();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING || event == OBS_FRONTEND_EVENT_PROFILE_RENAMED ||
		   event == OBS_FRONTEND_EVENT_EXIT) {
		md->SaveSettings();
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING || event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		md->mainStreamButton->setChecked(true);
		md->mainStreamButton->setStyleSheet(QString::fromUtf8("background: rgb(0,210,153);"));
		md->mainStreamButton->setIcon(md->streamActiveIcon);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		md->mainStreamButton->setChecked(false);
		md->mainStreamButton->setStyleSheet(QString::fromUtf8(""));
		md->mainStreamButton->setIcon(md->streamInactiveIcon);
	}
}

void MultistreamDock::LoadSettingsFile()
{

	obs_data_release(current_config);
	current_config = nullptr;
	char *path = obs_module_config_path("config.json");
	if (!path)
		return;
	obs_data_t *config = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!config) {
		config = obs_data_create();
		blog(LOG_WARNING, "[Aitum Multistream] No configuration file loaded");
	} else {
		blog(LOG_INFO, "[Aitum Multistream] Loaded configuration file");
	}
	char *profile = obs_frontend_get_current_profile();
	auto profiles = obs_data_get_array(config, "profiles");
	auto pc = obs_data_array_count(profiles);
	obs_data_t *pd = nullptr;
	for (size_t i = 0; i < pc; i++) {
		obs_data_t *t = obs_data_array_item(profiles, i);
		if (!t)
			continue;
		auto name = obs_data_get_string(t, "name");
		if (strcmp(profile, name) == 0) {
			pd = t;
			break;
		}
		obs_data_release(t);
	}
	obs_data_array_release(profiles);

	obs_data_release(config);
	if (!pd) {
		current_config = obs_data_create();
		obs_data_set_string(current_config, "name", profile);
		bfree(profile);
		blog(LOG_INFO, "[Aitum Multistream] profile not found");
		LoadSettings();
		return;
	}
	bfree(profile);
	current_config = pd;
	LoadSettings();
}

void MultistreamDock::LoadSettings()
{
	auto outputs = obs_data_get_array(current_config, "outputs");
	auto count = obs_data_array_count(outputs);
	int idx = 1;
	while (auto item = mainCanvasLayout->itemAt(idx)) {
		auto streamGroup = item->widget();
		auto name = streamGroup->objectName();
		bool found = false;
		for (size_t i = 0; i < count; i++) {
			auto item = obs_data_array_item(outputs, i);
			if (QString::fromUtf8(obs_data_get_string(item, "name")) == name) {
				found = true;
			}
			obs_data_release(item);
		}
		if (!found) {
			mainCanvasLayout->removeWidget(streamGroup);
			RemoveWidget(streamGroup);
		} else {
			idx++;
		}
	}

	obs_data_array_enum(
		outputs,
		[](obs_data_t *data, void *param) {
			auto d = (MultistreamDock *)param;
			d->LoadOutput(data);
		},
		this);
	obs_data_array_release(outputs);
}

void MultistreamDock::LoadOutput(obs_data_t *data)
{
	auto name = QString::fromUtf8(obs_data_get_string(data, "name"));
	for (int i = 1; i < mainCanvasLayout->count(); i++) {
		auto item = mainCanvasLayout->itemAt(i);
		if (item->widget()->objectName() == name) {
			return;
		}
	}
	auto streamGroup = new QGroupBox;
	streamGroup->setStyleSheet(QString("QGroupBox{background-color: %1; padding-top: 4px;}")
					   .arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));
	streamGroup->setObjectName(name);
	//mainStreamGroup->setStyleSheet(QString("QGroupBox{padding-top: 4px;}"));
	auto streamLayout = new QVBoxLayout;

	auto l2 = new QHBoxLayout;
	l2->addWidget(new QLabel(name), 1);
	auto streamButton = new QPushButton;
	streamButton->setMinimumHeight(30);
	streamButton->setObjectName(QStringLiteral("canvasStream"));
	streamButton->setIcon(streamInactiveIcon);
	streamButton->setCheckable(true);
	streamButton->setChecked(false);
	connect(streamButton, &QPushButton::clicked, [this, streamButton, data] {
		if (streamButton->isChecked()) {
			if (!StartOutput(data, streamButton))
				streamButton->setChecked(false);
		} else {
		}
		streamButton->setStyleSheet(QString::fromUtf8(streamButton->isChecked() ? "background: rgb(0,210,153);" : ""));
		streamButton->setIcon(streamButton->isChecked() ? streamActiveIcon : streamInactiveIcon);
	});
	//streamButton->setSizePolicy(sp2);
	streamButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	l2->addWidget(streamButton);
	streamLayout->addLayout(l2);

	streamGroup->setLayout(streamLayout);

	mainCanvasLayout->addWidget(streamGroup);
}

static void ensure_directory(char *path)
{
#ifdef _WIN32
	char *backslash = strrchr(path, '\\');
	if (backslash)
		*backslash = '/';
#endif

	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = 0;
		os_mkdirs(path);
		*slash = '/';
	}

#ifdef _WIN32
	if (backslash)
		*backslash = '\\';
#endif
}

void MultistreamDock::SaveSettings()
{
	char *path = obs_module_config_path("config.json");
	if (!path)
		return;
	obs_data_t *config = obs_data_create_from_json_file_safe(path, "bak");
	if (!config) {
		ensure_directory(path);
		config = obs_data_create();
		blog(LOG_WARNING, "[Aitum Multistream] New configuration file");
	}
	auto profiles = obs_data_get_array(config, "profiles");
	if (!profiles) {
		profiles = obs_data_array_create();
		obs_data_set_array(config, "profiles", profiles);
	}
	obs_data_t *pd = nullptr;
	if (current_config) {
		auto old_name = obs_data_get_string(current_config, "name");
		auto pc = obs_data_array_count(profiles);
		for (size_t i = 0; i < pc; i++) {
			obs_data_t *t = obs_data_array_item(profiles, i);
			if (!t)
				continue;
			auto name = obs_data_get_string(t, "name");
			if (strcmp(old_name, name) == 0) {
				pd = t;
				break;
			}
			obs_data_release(t);
		}
	}
	if (!pd) {
		pd = obs_data_create();
		obs_data_array_push_back(profiles, pd);
	}
	obs_data_array_release(profiles);
	char *profile = obs_frontend_get_current_profile();
	obs_data_set_string(pd, "name", profile);
	bfree(profile);
	if (current_config)
		obs_data_apply(pd, current_config);
	obs_data_release(pd);

	if (obs_data_save_json_safe(config, path, "tmp", "bak")) {
		blog(LOG_INFO, "[Aitum Multistream] Saved settings");
	} else {
		blog(LOG_ERROR, "[Aitum Multistream] Failed saving settings");
	}
	obs_data_release(config);
	bfree(path);
}

bool MultistreamDock::StartOutput(obs_data_t *settings, QPushButton *streamButton)
{
	const char *name = obs_data_get_string(settings, "name");
	auto old = outputs.find(name);
	if (old != outputs.end()) {
		auto service = obs_output_get_service(old->second);
		if (obs_output_active(old->second)) {
			obs_output_stop(old->second);
		}
		obs_output_release(old->second);
		obs_service_release(service);
		outputs.erase(old);
	}
	obs_encoder_t *venc = nullptr;
	obs_encoder_t *aenc = nullptr;
	auto advanced = obs_data_get_bool(settings, "advanced");
	if (advanced) {
		auto venc_name = obs_data_get_string(settings, "video_encoder");
		if (!venc_name || venc_name[0] == '\0') {
			//use main encoder
			auto main_output = obs_frontend_get_streaming_output();
			venc = obs_output_get_video_encoder2(main_output, obs_data_get_int(settings, "video_encoder_index"));
			if (!venc || !obs_output_active(main_output)) {
				obs_output_release(main_output);
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
						     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
			obs_output_release(main_output);
		} else {
			obs_data_t *s = nullptr;
			auto ves = obs_data_get_obj(settings, "video_encoder_settings");
			if (ves) {
				s = obs_data_create();
				obs_data_apply(s, ves);
				obs_data_release(ves);
			}
			venc = obs_video_encoder_create(venc_name, name, s, nullptr);
			obs_data_release(s);
			obs_encoder_set_video(venc, obs_get_video());
			auto divisor = obs_data_get_int(settings, "frame_rate_divisor");
			if (divisor > 1)
				obs_encoder_set_frame_rate_divisor(venc, divisor);

			bool scale = obs_data_get_bool(settings, "scale");
			if (scale) {
				obs_encoder_set_scaled_size(venc, obs_data_get_int(settings, "width"),
							    obs_data_get_int(settings, "height"));
				obs_encoder_set_gpu_scale_type(venc, (obs_scale_type)obs_data_get_int(settings, "scale_type"));
			}
		}
		auto aenc_name = obs_data_get_string(settings, "audio_encoder");
		if (!aenc_name || aenc_name[0] == '\0') {
			//use main encoder
			auto main_output = obs_frontend_get_streaming_output();
			aenc = obs_output_get_audio_encoder(main_output, obs_data_get_int(settings, "audio_encoder_index"));
			if (!aenc || !obs_output_active(main_output)) {
				obs_output_release(main_output);
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
						     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
			obs_output_release(main_output);
		} else {
			obs_data_t *s = nullptr;
			auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
			if (aes) {
				s = obs_data_create();
				obs_data_apply(s, aes);
				obs_data_release(aes);
			}
			aenc = obs_audio_encoder_create(venc_name, name, s, obs_data_get_int(settings, "audio_track"), nullptr);
			obs_data_release(s);
			obs_encoder_set_audio(aenc, obs_get_audio());
		}
	} else {
		auto main_output = obs_frontend_get_streaming_output();
		venc = main_output ? obs_output_get_video_encoder(main_output) : nullptr;
		if (!venc || !obs_output_active(main_output)) {
			obs_output_release(main_output);
			QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
					     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
			return false;
		}

		aenc = obs_output_get_audio_encoder(main_output, 0);
		obs_output_release(main_output);
	}
	if (!aenc || !venc) {
		return false;
	}

	auto s = obs_data_create();
	obs_data_set_string(s, "server", obs_data_get_string(settings, "server"));
	obs_data_set_string(s, "key", obs_data_get_string(settings, "key"));
	//use_auth
	//username
	//password
	auto service = obs_service_create("rtmp_custom", name, s, nullptr);
	obs_data_release(s);

	const char *type = obs_service_get_preferred_output_type(service);
	if (!type) {
		const char *url = obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
		type = "rtmp_output";
		if (url != NULL && strncmp(url, "ftl", 3) == 0) {
			type = "ftl_output";
		} else if (url != NULL && strncmp(url, "rtmp", 4) != 0) {
			type = "ffmpeg_mpegts_muxer";
		}
	}
	auto output = obs_output_create(type, name, nullptr, nullptr);
	obs_output_set_service(output, service);

	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "bind_ip", config_get_string(config, "Output", "BindIP"));
		obs_data_set_string(output_settings, "ip_family", config_get_string(config, "Output", "IPFamily"));
		obs_output_update(output, output_settings);
		obs_data_release(output_settings);

		bool useDelay = config_get_bool(config, "Output", "DelayEnable");
		int delaySec = config_get_int(config, "Output", "DelaySec");
		bool preserveDelay = config_get_bool(config, "Output", "DelayPreserve");
		obs_output_set_delay(output, useDelay ? delaySec : 0, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);
	}

	signal_handler_t *signal = obs_output_get_signal_handler(output);
	//signal_handler_disconnect(signal, "start", stream_output_start, streamButton);
	//signal_handler_disconnect(signal, "stop", stream_output_stop, streamButton);
	signal_handler_connect(signal, "start", stream_output_start, streamButton);
	signal_handler_connect(signal, "stop", stream_output_stop, streamButton);

	//for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
	//auto venc = obs_output_get_video_encoder2(main_output, 0);
	//for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
	//obs_output_get_audio_encoder(main_output, 0);

	obs_output_set_video_encoder(output, venc);
	obs_output_set_audio_encoder(output, aenc, 0);

	obs_output_start(output);

	outputs[obs_data_get_string(settings, "name")] = output;

	return true;
}

void MultistreamDock::stream_output_start(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	auto streamButton = (QPushButton *)data;
	streamButton->setChecked(true);
}

void MultistreamDock::stream_output_stop(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	auto streamButton = (QPushButton *)data;
	//const char *last_error = (const char *)calldata_ptr(calldata, "last_error");
	streamButton->setChecked(false);
}
