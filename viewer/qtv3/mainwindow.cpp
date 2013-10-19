// (c) by Stefan Roettger, licensed under GPL 2+

#include <iostream>

#include "mainwindow.h"
#include "mainconst.h"

QTV3MainWindow::QTV3MainWindow(QWidget *parent)
   : QMainWindow(parent)
{
   createMenus();
   createWidgets();
   createDocks();

   // accept drag and drop
   setAcceptDrops(true);

   setWindowTitle(APP_NAME" "APP_VERSION);

   vrw_->loadVolume("Drop.pvm","/usr/share/qtv3/");
   vrw_->setRotation(30.0);

   hasTeaserVolume_=true;

   flipXY1_=flipXY2_=0;
   flipYZ1_=flipYZ2_=0;

   clipNum_=0;
}

QTV3MainWindow::~QTV3MainWindow()
{
   delete vrw_;
   delete prefs_;
}

void QTV3MainWindow::loadVolume(const char *filename)
{
   vrw_->loadVolume(filename);

   if (label_)
   {
      mainLayout_->removeItem(mainLayout_->itemAt(0));
      delete label_;
      label_=NULL;
   }

   hasTeaserVolume_=false;
}

void QTV3MainWindow::loadSeries(const std::vector<std::string> list)
{
   vrw_->loadSeries(list);

   if (label_)
   {
      mainLayout_->removeItem(mainLayout_->itemAt(0));
      delete label_;
      label_=NULL;
   }

   hasTeaserVolume_=false;
}

void QTV3MainWindow::loadSurface(const char *filename)
{
   if (hasTeaserVolume_)
   {
      vrw_->clearVolume();
      hasTeaserVolume_=false;
   }

   vrw_->loadSurface(filename);

   if (label_)
   {
      mainLayout_->removeItem(mainLayout_->itemAt(0));
      delete label_;
      label_=NULL;
   }
}

void QTV3MainWindow::setRotation(double omega)
{
   vrw_->setRotation(omega);
}

void QTV3MainWindow::createMenus()
{
   QAction *quitAction = new QAction(tr("Q&uit"), this);
   quitAction->setShortcuts(QKeySequence::Quit);
   quitAction->setStatusTip(tr("Quit the application"));
   connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

   QAction *openAction = new QAction(tr("O&pen"), this);
   openAction->setShortcuts(QKeySequence::Open);
   openAction->setStatusTip(tr("Open Volume File"));
   connect(openAction, SIGNAL(triggered()), this, SLOT(open()));

   QAction *prefAction = new QAction(tr("P&references"), this);
   prefAction->setShortcuts(QKeySequence::Preferences);
   prefAction->setStatusTip(tr("Set Volume Rendering Preferences"));
   connect(prefAction, SIGNAL(triggered()), this, SLOT(prefs()));

   fileMenu_ = menuBar()->addMenu(tr("&File"));
   fileMenu_->addAction(openAction);
   fileMenu_->addAction(prefAction);
   fileMenu_->addAction(quitAction);

   QAction *aboutAction = new QAction(tr("&About"), this);
   aboutAction->setShortcut(tr("Ctrl+A"));
   aboutAction->setStatusTip(tr("About this program"));
   connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

   helpMenu_ = menuBar()->addMenu(tr("&Help"));
   helpMenu_->addAction(aboutAction);
}

void QTV3MainWindow::createWidgets()
{
   QGroupBox *mainGroup = new QGroupBox(this);
   mainLayout_ = new QVBoxLayout(mainGroup);

   mainSplitter_ = new QSplitter;
   QGroupBox *viewerGroup = new QGroupBox;
   viewerLayout_ = new QVBoxLayout;
   viewerSplitter_ = new QSplitter;
   QGroupBox *sliderGroup = new QGroupBox;
   sliderLayout_ = new QHBoxLayout;

   mainSplitter_->setOrientation(Qt::Vertical);

   mainSplitter_->addWidget(viewerGroup);
   mainSplitter_->addWidget(sliderGroup);
   viewerGroup->setLayout(viewerLayout_);
   sliderGroup->setLayout(sliderLayout_);

   mainLayout_->addWidget(mainSplitter_);
   mainGroup->setLayout(mainLayout_);

   viewerLayout_->addWidget(viewerSplitter_);
   viewerSplitter_->setOrientation(Qt::Horizontal);

   vrw_ = new QTV3VolRenWidget(viewerSplitter_);
   vrw_stereo_ = false;

   label_ = new QLabel("Drag and drop a volume file (.pvm .ima .dcm .rek .raw) into the window\n"
                       "to display it with the volume renderer!");

   label_->setAlignment(Qt::AlignHCenter);
   mainLayout_->insertWidget(0,label_);

   update_ = new QLabel("");
   update_->setAlignment(Qt::AlignHCenter);
   connect(vrw_, SIGNAL(update_signal(QString)), this, SLOT(update_slot(QString)));
   mainSplitter_->addWidget(update_);

   QTV3Slider *s1=createSlider(0,100,0,true);
   QTV3Slider *s2=createSlider(0,100,0,true);
   QTV3Slider *s3=createSlider(-180,180,0,false);
   QTV3Slider *s4=createSlider(-90,90,0,true);
   QTV3Slider *s5=createSlider(0,100,25,true);
   QTV3Slider *s6=createSlider(0,100,25,true);

   clipSlider_=s1;
   emiSlider_=s5;
   attSlider_=s6;

   connect(s1, SIGNAL(valueChanged(int)), this, SLOT(clip(int)));
   connect(s2, SIGNAL(valueChanged(int)), this, SLOT(zoom(int)));
   connect(s3, SIGNAL(valueChanged(int)), this, SLOT(rotate(int)));
   connect(s4, SIGNAL(valueChanged(int)), this, SLOT(tilt(int)));
   connect(s5, SIGNAL(valueChanged(int)), this, SLOT(emission(int)));
   connect(s6, SIGNAL(valueChanged(int)), this, SLOT(absorption(int)));

   QVBoxLayout *l1 = new QVBoxLayout;
   l1->addWidget(s1);
   QLabel *ll1=new QLabel("Clipping");
   ll1->setAlignment(Qt::AlignLeft);
   QPushButton *tackButton = new QPushButton(tr("Tack"));
   connect(tackButton, SIGNAL(pressed()), this, SLOT(tack()));
   QPushButton *clearButton = new QPushButton(tr("Clear"));
   connect(clearButton, SIGNAL(pressed()), this, SLOT(clear()));
   l1->addWidget(ll1);
   l1->addWidget(tackButton);
   l1->addWidget(clearButton);

   QGroupBox *g1 = new QGroupBox;
   g1->setLayout(l1);
   viewerSplitter_->addWidget(g1);

   QVBoxLayout *l2 = new QVBoxLayout;
   l2->addWidget(s2);
   QLabel *ll2=new QLabel("Zoom");
   ll2->setAlignment(Qt::AlignHCenter);
   l2->addWidget(ll2);
   sliderLayout_->addLayout(l2);

   QVBoxLayout *l3 = new QVBoxLayout;
   QLabel *ll3=new QLabel("Rotation");
   ll3->setAlignment(Qt::AlignHCenter);
   l3->addWidget(ll3);
   l3->addStretch(1000);
   l3->addWidget(s3);
   l3->addStretch(1000);
   QHBoxLayout *h1 = new QHBoxLayout;
   QCheckBox *gradMagCheck = new QCheckBox(tr("Gradient Magnitude"));
   gradMagCheck->setChecked(false);
   connect(gradMagCheck, SIGNAL(stateChanged(int)), this, SLOT(checkGradMag(int)));
   h1->addWidget(gradMagCheck);
   QCheckBox *invModeCheck = new QCheckBox(tr("Inverse Mode"));
   invModeCheck->setChecked(false);
   connect(invModeCheck, SIGNAL(stateChanged(int)), this, SLOT(checkInvMode(int)));
   h1->addWidget(invModeCheck);
   QHBoxLayout *h2 = new QHBoxLayout;
   QRadioButton *sfxOffCheck = new QRadioButton(tr("Normal Rendering"));
   sfxOffCheck->setChecked(true);
   connect(sfxOffCheck, SIGNAL(toggled(bool)), this, SLOT(checkSFXoff(bool)));
   h2->addWidget(sfxOffCheck);
   QRadioButton *anaModeCheck = new QRadioButton(tr("Anaglyph Stereo Mode"));
   anaModeCheck->setChecked(false);
   connect(anaModeCheck, SIGNAL(toggled(bool)), this, SLOT(checkAnaMode(bool)));
   h2->addWidget(anaModeCheck);
   QRadioButton *sfxOnCheck = new QRadioButton(tr("Dual Buffer Stereo Mode"));
   sfxOnCheck->setChecked(false);
   connect(sfxOnCheck, SIGNAL(toggled(bool)), this, SLOT(checkSFXon(bool)));
   h2->addWidget(sfxOnCheck);
   QHBoxLayout *h3 = new QHBoxLayout;
   QCheckBox *flipCheckXY1 = new QCheckBox(tr("Flip +XY"));
   flipCheckXY1->setChecked(false);
   connect(flipCheckXY1, SIGNAL(stateChanged(int)), this, SLOT(checkFlipXY1(int)));
   h3->addWidget(flipCheckXY1);
   QCheckBox *flipCheckXY2 = new QCheckBox(tr("Flip -XY"));
   flipCheckXY2->setChecked(false);
   connect(flipCheckXY2, SIGNAL(stateChanged(int)), this, SLOT(checkFlipXY2(int)));
   h3->addWidget(flipCheckXY2);
   QCheckBox *flipCheckYZ1 = new QCheckBox(tr("Flip +YZ"));
   flipCheckYZ1->setChecked(false);
   connect(flipCheckYZ1, SIGNAL(stateChanged(int)), this, SLOT(checkFlipYZ1(int)));
   h3->addWidget(flipCheckYZ1);
   QCheckBox *flipCheckYZ2 = new QCheckBox(tr("Flip -YZ"));
   flipCheckYZ2->setChecked(false);
   connect(flipCheckYZ2, SIGNAL(stateChanged(int)), this, SLOT(checkFlipYZ2(int)));
   h3->addWidget(flipCheckYZ2);
   QHBoxLayout *h4 = new QHBoxLayout;
   QRadioButton *sampleButton1 = new QRadioButton(tr("Undersampling"), this);
   QRadioButton *sampleButton2 = new QRadioButton(tr("Regular Sampling"), this);
   QRadioButton *sampleButton3 = new QRadioButton(tr("Oversampling"), this);
   sampleButton2->setChecked(true);
   connect(sampleButton1, SIGNAL(toggled(bool)), this, SLOT(samplingChanged1(bool)));
   connect(sampleButton2, SIGNAL(toggled(bool)), this, SLOT(samplingChanged2(bool)));
   connect(sampleButton3, SIGNAL(toggled(bool)), this, SLOT(samplingChanged3(bool)));
   h4->addWidget(sampleButton1);
   h4->addWidget(sampleButton2);
   h4->addWidget(sampleButton3);
   l3->addLayout(h1);
   l3->addLayout(h2);
   l3->addLayout(h3);
   l3->addLayout(h4);
   sliderLayout_->addLayout(l3);

   QVBoxLayout *l4 = new QVBoxLayout;
   l4->addWidget(s4);
   QLabel *ll4=new QLabel("Tilt");
   ll4->setAlignment(Qt::AlignHCenter);
   l4->addWidget(ll4);
   sliderLayout_->addLayout(l4);

   QFrame* line1 = new QFrame();
   line1->setFrameShape(QFrame::VLine);
   line1->setFrameShadow(QFrame::Raised);
   sliderLayout_->addWidget(line1);

   QVBoxLayout *l5 = new QVBoxLayout;
   l5->addWidget(s5);
   QLabel *ll5=new QLabel("Emission");
   ll5->setAlignment(Qt::AlignHCenter);
   l5->addWidget(ll5);
   sliderLayout_->addLayout(l5);

   QVBoxLayout *l6 = new QVBoxLayout;
   l6->addWidget(s6);
   QLabel *ll6=new QLabel("Absorption");
   ll6->setAlignment(Qt::AlignHCenter);
   l6->addWidget(ll6);
   sliderLayout_->addLayout(l6);

   QFrame* line2 = new QFrame();
   line2->setFrameShape(QFrame::VLine);
   line2->setFrameShadow(QFrame::Raised);
   sliderLayout_->addWidget(line2);

   QVBoxLayout *l7 = new QVBoxLayout;
   QPushButton *isoButton = new QPushButton(tr("Extract"));
   connect(isoButton, SIGNAL(pressed()), this, SLOT(extractSurface()));
   l7->addWidget(isoButton);
   QPushButton *isoClearButton = new QPushButton(tr("Clear"));
   connect(isoClearButton, SIGNAL(pressed()), this, SLOT(clearSurface()));
   l7->addWidget(isoClearButton);
   QLabel *ll7=new QLabel("Iso Surface");
   ll7->setAlignment(Qt::AlignHCenter);
   l7->addStretch(100);
   l7->addWidget(ll7);
   sliderLayout_->addLayout(l7);

   viewerSplitter_->addWidget(vrw_);

   setCentralWidget(mainGroup);
}

void QTV3MainWindow::createDocks()
{
   prefs_ = new QTV3PrefWindow(this, vrw_);
   prefs_->setAllowedAreas(Qt::RightDockWidgetArea);

   addDockWidget(Qt::RightDockWidgetArea, prefs_);
   prefs_->hide();
}

QStringList QTV3MainWindow::browse(QString path,
                                   bool newfile)
{
   QFileDialog* fd = new QFileDialog(this, "File Selector Dialog");

   if (!newfile) fd->setFileMode(QFileDialog::ExistingFiles);
   else fd->setFileMode(QFileDialog::AnyFile);
   fd->setViewMode(QFileDialog::List);
   if (newfile) fd->setAcceptMode(QFileDialog::AcceptSave);
   fd->setFilter("All Files (*);;GIF Files (*.gif);;JPEG Files (*.jpg);;TIFF Files(*.tif *.tiff)");

   if (path!="") fd->setDirectory(path);

   QStringList files;

   if (fd->exec() == QDialog::Accepted)
      for (int i=0; i<fd->selectedFiles().size(); i++)
      {
         QString fileName = fd->selectedFiles().at(i);

         if (!fileName.isNull())
            files += fileName;
      }

   delete fd;

   return(files);
}

QTV3Slider *QTV3MainWindow::createSlider(int minimum, int maximum, int value,
                                         bool vertical)
{
   QTV3Slider *slider = new QTV3Slider(vertical?Qt::Vertical:Qt::Horizontal);
   slider->setRange(minimum * 16, maximum * 16);
   slider->setSingleStep(16);
   slider->setPageStep((maximum - minimum) / 10 * 16);
   slider->setTickInterval((maximum - minimum) / 10 * 16);
   slider->setTickPosition(QSlider::TicksBelow);
   slider->setValue(value * 16);
   return(slider);
}

QSize QTV3MainWindow::minimumSizeHint() const
{
   return(QSize(100, 100));
}

QSize QTV3MainWindow::sizeHint() const
{
   return(QSize(MAIN_WIDTH, MAIN_HEIGHT));
}

void QTV3MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
   event->acceptProposedAction();
}

void QTV3MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
   event->acceptProposedAction();
}

void QTV3MainWindow::dropEvent(QDropEvent *event)
{
   const QMimeData *mimeData = event->mimeData();

   if (mimeData->hasUrls())
   {
      event->acceptProposedAction();

      QList<QUrl> urlList = mimeData->urls();

      if (urlList.size()==1)
      {
         QUrl qurl = urlList.at(0);
         QString url = qurl.toString();

         if (url.startsWith("file://"))
         {
            url = normalizeFile(url);

            if (url.endsWith(".geo"))
            {
               loadSurface(url.toStdString().c_str());
            }
            else
            {
               loadVolume(url.toStdString().c_str());
            }
         }
      }
      else
      {
         std::vector<std::string> list;

         for (int i=0; i<urlList.size(); i++)
         {
            QUrl qurl = urlList.at(i);
            QString url = qurl.toString();

            if (url.startsWith("file://"))
            {
               list.push_back(normalizeFile(url).toStdString());
            }
         }

         loadSeries(list);
      }
   }
}

void QTV3MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
   event->accept();
}

QString QTV3MainWindow::normalizeFile(QString file)
{
   file.remove("file://");
   file.replace('\\', '/');
   if (file.contains(QRegExp("^/[A-Z]:"))) file.remove(0,1);

   return(file);
}

void QTV3MainWindow::open()
{
   QStringList files = browse();

   if (files.size()==1)
   {
      QString file = normalizeFile(files.at(0));

      if (file.endsWith(".geo"))
      {
         loadSurface(file.toStdString().c_str());
      }
      else
      {
         loadVolume(file.toStdString().c_str());
      }
   }
   else
   {
      std::vector<std::string> list;

      for (int i=0; i<files.size(); i++)
      {
         list.push_back(normalizeFile(files.at(i)).toStdString());
      }

      loadSeries(list);
   }
}

void QTV3MainWindow::zoom(int v)
{
   double zoom = v / 16.0 / 100.0;
   vrw_->setZoom(zoom);
}

void QTV3MainWindow::rotate(int v)
{
   double angle = v / 16.0;
   vrw_->setAngle(angle);
}

void QTV3MainWindow::tilt(int v)
{
   double tilt = v / 16.0;
   vrw_->setTilt(tilt);
}

void QTV3MainWindow::clip(int v)
{
   double dist = v / 16.0 / 100.0;
   vrw_->setClipDist(1.0-2*dist);
}

void QTV3MainWindow::tack()
{
   double px,py,pz;
   double nx,ny,nz;

   vrw_->getNearPlane(px,py,pz, nx,ny,nz);
   vrw_->setClipPlane(clipNum_, px,py,pz, nx,ny,nz);
   vrw_->enableClipPlane(clipNum_,1);

   clipSlider_->setValue(0);

   clipNum_++;
   if (clipNum_>=6) clear();
}

void QTV3MainWindow::clear()
{
   vrw_->disableClipPlanes();

   clipNum_=0;

   clipSlider_->setValue(0);
}

void QTV3MainWindow::emission(int v)
{
   double emi = v / 16.0 / 100.0;
   vrw_->setEmission(emi);
}

void QTV3MainWindow::absorption(int v)
{
   double att = v / 16.0 / 100.0;
   vrw_->setAbsorption(att);
}

void QTV3MainWindow::checkGradMag(int on)
{
   vrw_->setGradMag(on);
   emiSlider_->setValue(16*100*vrw_->getEmission());
   attSlider_->setValue(16*100*vrw_->getAbsorption());
}

void QTV3MainWindow::checkInvMode(int on)
{
   vrw_->setInvMode(on);
}

void QTV3MainWindow::checkSFX(bool stereo)
{
   if (stereo!=vrw_stereo_)
   {
      delete vrw_;
      vrw_ = new QTV3VolRenWidget(viewerSplitter_, stereo);
      connect(vrw_, SIGNAL(update_signal(QString)), this, SLOT(update_slot(QString)));
      viewerSplitter_->addWidget(vrw_);

      vrw_stereo_ = stereo;
   }
}

void QTV3MainWindow::checkSFXoff(bool on)
{
   checkSFX(false);

   if (on)
      vrw_->setSFX(false);
}

void QTV3MainWindow::checkAnaMode(bool on)
{
   checkSFX(false);

   if (on)
   {
      vrw_->setSFX(true);
      vrw_->setAnaglyph(on);
   }
}

void QTV3MainWindow::checkSFXon(bool on)
{
   checkSFX(true);

   if (on)
   {
      delete vrw_;
      vrw_ = new QTV3VolRenWidget(viewerSplitter_, true);
      connect(vrw_, SIGNAL(update_signal(QString)), this, SLOT(update_slot(QString)));
      viewerSplitter_->addWidget(vrw_);

      vrw_->setSFX(true);
      vrw_->setAnaglyph(false);

      delete prefs_;
      createDocks();
   }
}

void QTV3MainWindow::setTilt()
{
   double tiltXY=0.0;
   double tiltYZ=0.0;

   if (flipXY1_) tiltXY+=90.0;
   if (flipXY2_) tiltXY-=90.0;

   if (flipYZ1_) tiltYZ+=90.0;
   if (flipYZ2_) tiltYZ-=90.0;

   if (fabs(tiltXY)>0.0 && fabs(tiltYZ)>0.0)
   {
      vrw_->setTiltXY(180.0);
      vrw_->setTiltYZ(0.0);
   }
   else
   {
      vrw_->setTiltXY(tiltXY);
      vrw_->setTiltYZ(tiltYZ);
   }
}

void QTV3MainWindow::checkFlipXY1(int on)
{
   flipXY1_=on;
   setTilt();
}

void QTV3MainWindow::checkFlipXY2(int on)
{
   flipXY2_=on;
   setTilt();
}

void QTV3MainWindow::checkFlipYZ1(int on)
{
   flipYZ1_=on;
   setTilt();
}

void QTV3MainWindow::checkFlipYZ2(int on)
{
   flipYZ2_=on;
   setTilt();
}

void QTV3MainWindow::samplingChanged1(bool on)
{
   if (on) vrw_->setOversampling(0.5);
}

void QTV3MainWindow::samplingChanged2(bool on)
{
   if (on) vrw_->setOversampling(1.0);
}

void QTV3MainWindow::samplingChanged3(bool on)
{
   if (on) vrw_->setOversampling(2.0);
}

void QTV3MainWindow::extractSurface()
{
   vrw_->extractSurface();
}

void QTV3MainWindow::clearSurface()
{
   vrw_->clearSurface();
}

void QTV3MainWindow::prefs()
{
   prefs_->show();
}

void QTV3MainWindow::about()
{
   QMessageBox::about(this, tr("About this program"),
                      APP_NAME" "APP_VERSION
                      "\n"
                      "\n"APP_LICENSE
                      "\n"APP_COPYRIGHT
                      "\n"
                      "\n"APP_DISCLAIMER);
}

void QTV3MainWindow::update_slot(QString text)
{
   update_->setText(text);
}
