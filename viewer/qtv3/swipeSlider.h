// (c) by Stefan Roettger, licensed under GPL 3.0

#ifndef SWIPESLIDER_H
#define SWIPESLIDER_H

#ifdef HAVE_QT5
#include <QtWidgets>
#else
#include <QtGui>
#endif

#include "swipeFilter.h"

//! swiping slider widget
class SwipeSlider: public QWidget
{
   Q_OBJECT

public:

   SwipeSlider(Qt::Orientation orientation, QString text = "", QWidget *parent = NULL);
   virtual ~SwipeSlider();

   void setRange(double minimum, double maximum);

   void enableSwipe();
   void disableSwipe();

   void scrollSlider(double offset);

   double getValue();
   double getNormalizedValue();

   void setValue(double value);
   void setNormalizedValue(double value);

   void setOutline(int width=0);
   void setBlackOnWhite(bool white=true);

   //! return preferred window size
   QSize sizeHint() const
   {
      return(minimumSizeHint());
   }

   //! return preferred minimum window size
   QSize minimumSizeHint() const
   {
      if (orientation_ == Qt::Vertical)
         return(QSize(minsize_, heightForWidth(minsize_)));
      else
         return(QSize(widthForHeight(minsize_), minsize_));
   }

   //! return height for given width
   int heightForWidth(int x) const
   {
      int y, miny;

      if (orientation_ == Qt::Vertical)
      {
         y = x*aspect_;
         miny = minsize_*aspect_;
      }
      else
      {
         y = x/aspect_;
         miny = minsize_;
      }

      if (y < miny) y = miny;

      return(y);
   }

   //! return width for given height
   int widthForHeight(int y) const
   {
      int x, minx;

      if (orientation_ == Qt::Vertical)
      {
         x = y/aspect_;
         minx = minsize_;
      }
      else
      {
         x = y*aspect_;
         minx = minsize_*aspect_;
      }

      if (x < minx) x = minx;

      return(x);
   }

   //! configure aspect ratio of slider
   void setAspect(double aspect, double minsize = 100)
   {
      aspect_ = aspect;
      minsize_ = minsize;
   }

protected:

   double value_;
   bool set_;

   double minimum_;
   double maximum_;

   SwipeFilter *filter;
   bool enabled;

   double aspect_;
   int minsize_;

   Qt::Orientation orientation_;
   QString text_;
   int outline_;
   bool white_;

   virtual void paintEvent(QPaintEvent *event);

protected slots:

   void move(SwipeDirection direction, int offset);
   void kinetic(SwipeDirection direction, int offset);

signals:

   void valueChanged(double value);
};

#endif
