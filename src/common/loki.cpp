#include "loki.h"
#include <assert.h>

/* Exponential base 2 function.
   Copyright (C) 2012-2019 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */

#include <limits>
#include <cfloat>
#include <cmath>

#include <algorithm>
#include <vector>

// TODO(loki): This is temporary until we switch to integer math for calculating
// block rewards. We provide the specific implementation to minimise the risk of
// different results from math functions across different std libraries.
static_assert(std::numeric_limits<double>::is_iec559, "We require IEEE standard compliant doubles.");

/* Best possible approximation of log(2) as a 'double'.  */
#define LOG2 0.693147180559945309417232121458176568075

/* Best possible approximation of 1/log(2) as a 'double'.  */
#define LOG2_INVERSE 1.44269504088896340735992468100189213743

/* Best possible approximation of log(2)/256 as a 'double'.  */
#define LOG2_BY_256 0.00270760617406228636491106297444600221904

/* Best possible approximation of 256/log(2) as a 'double'.  */
#define LOG2_BY_256_INVERSE 369.329930467574632284140718336484387181

double
loki::exp2(double x)
{
  /* exp2(x) = exp(x*log(2)).
     If we would compute it like this, there would be rounding errors for
     integer or near-integer values of x.  To avoid these, we inline the
     algorithm for exp(), and the multiplication with log(2) cancels a
     division by log(2).  */

  // if (isnand (x)) // unnecessary for us
  // return x;

  if (x > (double) DBL_MAX_EXP)
    /* x > DBL_MAX_EXP
       hence exp2(x) > 2^DBL_MAX_EXP, overflows to Infinity.  */
    return HUGE_VAL;

  if (x < (double) (DBL_MIN_EXP - 1 - DBL_MANT_DIG))
    /* x < (DBL_MIN_EXP - 1 - DBL_MANT_DIG)
       hence exp2(x) < 2^(DBL_MIN_EXP-1-DBL_MANT_DIG),
       underflows to zero.  */
    return 0.0;

  /* Decompose x into
       x = n + m/256 + y/log(2)
     where
       n is an integer,
       m is an integer, -128 <= m <= 128,
       y is a number, |y| <= log(2)/512 + epsilon = 0.00135...
     Then
       exp2(x) = 2^n * exp(m * log(2)/256) * exp(y)
     The first factor is an ldexpl() call.
     The second factor is a table lookup.
     The third factor is computed
     - either as sinh(y) + cosh(y)
       where sinh(y) is computed through the power series:
         sinh(y) = y + y^3/3! + y^5/5! + ...
       and cosh(y) is computed as hypot(1, sinh(y)),
     - or as exp(2*z) = (1 + tanh(z)) / (1 - tanh(z))
       where z = y/2
       and tanh(z) is computed through its power series:
         tanh(z) = z
                   - 1/3 * z^3
                   + 2/15 * z^5
                   - 17/315 * z^7
                   + 62/2835 * z^9
                   - 1382/155925 * z^11
                   + 21844/6081075 * z^13
                   - 929569/638512875 * z^15
                   + ...
       Since |z| <= log(2)/1024 < 0.0007, the relative contribution of the
       z^7 term is < 0.0007^6 < 2^-60 <= 2^-DBL_MANT_DIG, therefore we can
       truncate the series after the z^5 term.  */

  {
    double nm = loki::round (x * 256.0); /* = 256 * n + m */
    double z = (x * 256.0 - nm) * (LOG2_BY_256 * 0.5);

/* Coefficients of the power series for tanh(z).  */
#define TANH_COEFF_1   1.0
#define TANH_COEFF_3  -0.333333333333333333333333333333333333334
#define TANH_COEFF_5   0.133333333333333333333333333333333333334
#define TANH_COEFF_7  -0.053968253968253968253968253968253968254
#define TANH_COEFF_9   0.0218694885361552028218694885361552028218
#define TANH_COEFF_11 -0.00886323552990219656886323552990219656886
#define TANH_COEFF_13  0.00359212803657248101692546136990581435026
#define TANH_COEFF_15 -0.00145583438705131826824948518070211191904

    double z2 = z * z;
    double tanh_z =
      ((TANH_COEFF_5
        * z2 + TANH_COEFF_3)
       * z2 + TANH_COEFF_1)
      * z;

    double exp_y = (1.0 + tanh_z) / (1.0 - tanh_z);

    int n = (int) loki::round (nm * (1.0 / 256.0));
    int m = (int) nm - 256 * n;

    /* exp_table[i] = exp((i - 128) * log(2)/256).
       Computed in GNU clisp through
         (setf (long-float-digits) 128)
         (setq a 0L0)
         (setf (long-float-digits) 256)
         (dotimes (i 257)
           (format t "        ~D,~%"
                   (float (exp (* (/ (- i 128) 256) (log 2L0))) a)))  */
    static const double exp_table[257] =
      {
        0.707106781186547524400844362104849039284,
        0.709023942160207598920563322257676190836,
        0.710946301084582779904674297352120049962,
        0.71287387205274715340350157671438300618,
        0.714806669195985005617532889137569953044,
        0.71674470668389442125974978427737336719,
        0.71868799872449116280161304224785251353,
        0.720636559564312831364255957304947586072,
        0.72259040348852331001850312073583545284,
        0.724549544821017490259402705487111270714,
        0.726513997924526282423036245842287293786,
        0.728483777200721910815451524818606761737,
        0.730458897090323494325651445155310766577,
        0.732439372073202913296664682112279175616,
        0.734425216668490963430822513132890712652,
        0.736416445434683797507470506133110286942,
        0.738413072969749655693453740187024961962,
        0.740415113911235885228829945155951253966,
        0.742422582936376250272386395864403155277,
        0.744435494762198532693663597314273242753,
        0.746453864145632424600321765743336770838,
        0.748477705883617713391824861712720862423,
        0.750507034813212760132561481529764324813,
        0.752541865811703272039672277899716132493,
        0.75458221379671136988300977551659676571,
        0.756628093726304951096818488157633113612,
        0.75867952059910734940489114658718937343,
        0.760736509454407291763130627098242426467,
        0.762799075372269153425626844758470477304,
        0.76486723347364351194254345936342587308,
        0.766940998920478000900300751753859329456,
        0.769020386915828464216738479594307884331,
        0.771105412703970411806145931045367420652,
        0.773196091570510777431255778146135325272,
        0.77529243884249997956151370535341912283,
        0.777394469888544286059157168801667390437,
        0.779502200118918483516864044737428940745,
        0.781615644985678852072965367573877941354,
        0.783734819982776446532455855478222575498,
        0.78585974064617068462428149076570281356,
        0.787990422553943243227635080090952504452,
        0.790126881326412263402248482007960521995,
        0.79226913262624686505993407346567890838,
        0.794417192158581972116898048814333564685,
        0.796571075671133448968624321559534367934,
        0.798730798954313549131410147104316569576,
        0.800896377841346676896923120795476813684,
        0.803067828208385462848443946517563571584,
        0.805245165974627154089760333678700291728,
        0.807428407102430320039984581575729114268,
        0.809617567597431874649880866726368203972,
        0.81181266350866441589760797777344082227,
        0.814013710928673883424109261007007338614,
        0.816220725993637535170713864466769240053,
        0.818433724883482243883852017078007231025,
        0.82065272382200311435413206848451310067,
        0.822877739076982422259378362362911222833,
        0.825108786960308875483586738272485101678,
        0.827345883828097198786118571797909120834,
        0.829589046080808042697824787210781231927,
        0.831838290163368217523168228488195222638,
        0.834093632565291253329796170708536192903,
        0.836355089820798286809404612069230711295,
        0.83862267850893927589613232455870870518,
        0.84089641525371454303112547623321489504,
        0.84317631672419664796432298771385230143,
        0.84546239963465259098692866759361830709,
        0.84775468074466634749045860363936420312,
        0.850053176859261734750681286748751167545,
        0.852357904829025611837203530384718316326,
        0.854668881550231413551897437515331498025,
        0.856986123964963019301812477839166009452,
        0.859309649061238957814672188228156252257,
        0.861639473873136948607517116872358729753,
        0.863975615480918781121524414614366207052,
        0.866318091011155532438509953514163469652,
        0.868666917636853124497101040936083380124,
        0.871022112577578221729056715595464682243,
        0.873383693099584470038708278290226842228,
        0.875751676515939078050995142767930296012,
        0.878126080186649741556080309687656610647,
        0.880506921518791912081045787323636256171,
        0.882894217966636410521691124969260937028,
        0.885287987031777386769987907431242017412,
        0.88768824626326062627527960009966160388,
        0.89009501325771220447985955243623523504,
        0.892508305659467490072110281986409916153,
        0.8949281411607004980029443898876582985,
        0.897354537501553593213851621063890907178,
        0.899787512470267546027427696662514569756,
        0.902227083903311940153838631655504844215,
        0.904673269685515934269259325789226871994,
        0.907126087750199378124917300181170171233,
        0.909585556079304284147971563828178746372,
        0.91205169270352665549806275316460097744,
        0.914524515702448671545983912696158354092,
        0.91700404320467123174354159479414442804,
        0.919490293387946858856304371174663918816,
        0.921983284479312962533570386670938449637,
        0.92448303475522546419252726694739603678,
        0.92698956254169278419622653516884831976,
        0.929502886214410192307650717745572682403,
        0.932023024198894522404814545597236289343,
        0.934549994970619252444512104439799143264,
        0.93708381705514995066499947497722326722,
        0.93962450902828008902058735120448448827,
        0.942172089516167224843810351983745154882,
        0.944726577195469551733539267378681531548,
        0.947287990793482820670109326713462307376,
        0.949856349088277632361251759806996099924,
        0.952431670908837101825337466217860725517,
        0.955013975135194896221170529572799135168,
        0.957603280698573646936305635147915443924,
        0.960199606581523736948607188887070611744,
        0.962802971818062464478519115091191368377,
        0.965413395493813583952272948264534783197,
        0.968030896746147225299027952283345762418,
        0.970655494764320192607710617437589705184,
        0.973287208789616643172102023321302921373,
        0.97592605811548914795551023340047499377,
        0.978572062087700134509161125813435745597,
        0.981225240104463713381244885057070325016,
        0.983885611616587889056366801238014683926,
        0.98655319612761715646797006813220671315,
        0.989228013193975484129124959065583667775,
        0.99191008242510968492991311132615581644,
        0.994599423483633175652477686222166314457,
        0.997296056085470126257659913847922601123,
        1.0,
        1.00271127505020248543074558845036204047,
        1.0054299011128028213513839559347998147,
        1.008155898118417515783094890817201039276,
        1.01088928605170046002040979056186052439,
        1.013630084951489438840258929063939929597,
        1.01637831491095303794049311378629406276,
        1.0191339960777379496848780958207928794,
        1.02189714865411667823448013478329943978,
        1.02466779289713564514828907627081492763,
        1.0274459491187636965388611939222137815,
        1.030231637686041012871707902453904567093,
        1.033024879021228422500108283970460918086,
        1.035825693601957120029983209018081371844,
        1.03863410196137879061243669795463973258,
        1.04145012468831614126454607901189312648,
        1.044273782427413840321966478739929008784,
        1.04710509587928986612990725022711224056,
        1.04994408580068726608203812651590790906,
        1.05279077300462632711989120298074630319,
        1.05564517836055715880834132515293865216,
        1.058507322794512690105772109683716645074,
        1.061377227289262080950567678003883726294,
        1.06425491288446454978861125700158022068,
        1.06714040067682361816952112099280916261,
        1.0700337118202417735424119367576235685,
        1.072934867525975551385035450873827585343,
        1.075843889062791037803228648476057074063,
        1.07876079775711979374068003743848295849,
        1.081685614993215201942115594422531125643,
        1.08461836221330923781610517190661434161,
        1.087559060917769665346797830944039707867,
        1.09050773266525765920701065576070797899,
        1.09346439907288585422822014625044716208,
        1.096429081816376823386138295859248481766,
        1.09940180263022198546369696823882990404,
        1.10238258330784094355641420942564685751,
        1.10537144570174125558827469625695031104,
        1.108368411723678638009423649426619850137,
        1.111373503344817603850149254228916637444,
        1.1143867425958925363088129569196030678,
        1.11740815156736919905457996308578026665,
        1.12043775240960668442900387986631301277,
        1.123475567333019800733729739775321431954,
        1.12652161860824189979479864378703477763,
        1.129575928566288145997264988840249825907,
        1.13263851959871922798707372367762308438,
        1.13570941415780551424039033067611701343,
        1.13878863475669165370383028384151125472,
        1.14187620396956162271229760828788093894,
        1.14497214443180421939441388822291589579,
        1.14807647884017900677879966269734268003,
        1.15118922995298270581775963520198253612,
        1.154310420590216039548221528724806960684,
        1.157440073633751029613085766293796821106,
        1.16057821202749874636945947257609098625,
        1.16372485877757751381357359909218531234,
        1.166880036952481570555516298414089287834,
        1.170043769683250188080259035792738573,
        1.17321608016363724753480435451324538889,
        1.176396991650281276284645728483848641054,
        1.17958652746287594548610056676944051898,
        1.182784710984341029924457204693850757966,
        1.18599156566099383137126564953421556374,
        1.18920711500272106671749997056047591529,
        1.19243138258315122214272755814543101148,
        1.195664392039827374583837049865451975705,
        1.19890616707438048177030255797630020695,
        1.202156731452703142096396957497765876003,
        1.205416109005123825604211432558411335666,
        1.208684323626581577354792255889216998484,
        1.21196139927680119446816891773249304545,
        1.215247359980468878116520251338798457624,
        1.218542229827408361758207148117394510724,
        1.221846032972757516903891841911570785836,
        1.225158793637145437709464594384845353707,
        1.22848053610687000569400895779278184036,
        1.2318112847340759358845566532127948166,
        1.235151063936933305692912507415415760294,
        1.238499898199816567833368865859612431545,
        1.24185781207348404859367746872659560551,
        1.24522483017525793277520496748615267417,
        1.24860097718920473662176609730249554519,
        1.25198627786631627006020603178920359732,
        1.255380757024691089579390657442301194595,
        1.25878443954971644307786044181516261876,
        1.26219735039425070801401025851841645967,
        1.265619514578806324196273999873453036296,
        1.26905095719173322255441908103233800472,
        1.27249170338940275123669204418460217677,
        1.27594177839639210038120243475928938891,
        1.27940120750566922691358797002785254596,
        1.28287001607877828072666978102151405111,
        1.286348229546025533601482208069738348355,
        1.28983587340666581223274729549155218968,
        1.293332973229089436725559789048704304684,
        1.296839554651009665933754117792451159835,
        1.30035564337965065101414056707091779129,
        1.30388126519193589857452364895199736833,
        1.30741644593467724479715157747196172848,
        1.310961211524764341922991786330755849366,
        1.314515587949354658485983613383997794965,
        1.318079601266063994690185647066116617664,
        1.32165327760315751432651181233060922616,
        1.32523664315974129462953709549872167411,
        1.32882972420595439547865089632866510792,
        1.33243254708316144935164337949073577407,
        1.33604513820414577344262790437186975929,
        1.33966752405330300536003066972435257602,
        1.34329973118683526382421714618163087542,
        1.346941786232945835788173713229537282075,
        1.35059371589203439140852219606013396004,
        1.35425554693689272829801474014070280434,
        1.357927306212901046494536695671766697446,
        1.36160902063822475558553593883194147464,
        1.36530071720401181543069836033754285543,
        1.36900242297459061192960113298219283217,
        1.37271416508766836928499785714471721579,
        1.37643597075453010021632280551868696026,
        1.380167867260238095581945274358283464697,
        1.383909881963831954872659527265192818,
        1.387662042298529159042861017950775988896,
        1.39142437577192618714983552956624344668,
        1.395196909966200178275574599249220994716,
        1.398979672538311140209528136715194969206,
        1.40277269122020470637471352433337881711,
        1.40657599381901544248361973255451684411,
        1.410389608217270704414375128268675481145,
        1.41421356237309504880168872420969807857
      };

    double ret = exp_table[128 + m] * exp_y;
    for (int i = 0; i < n; i++)
      ret *= 2;
    return ret;
  }
}

/* Round toward nearest, breaking ties away from zero.
   Copyright (C) 2007, 2010-2019 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License along
   with this program; if not, see <https://www.gnu.org/licenses/>.  */

/* Written by Ben Pfaff <blp@gnu.org>, 2007.
   Based heavily on code by Bruno Haible. */

/* Specification.  */

#include <cstdint>
#include <cfloat>
#include <limits>

/* -0.0.  See minus-zero.h.  */
#if defined __hpux || defined __sgi || defined __ICC
# define MINUS_ZERO (-DBL_MIN * DBL_MIN)
#else
# define MINUS_ZERO -0.0
#endif

/* MSVC with option -fp:strict refuses to compile constant initializers that
   contain floating-point operations.  Pacify this compiler.  */
#ifdef _MSC_VER
# pragma fenv_access (off)
#endif

double
loki::round (double x)
{
  /* 2^(DBL_MANT_DIG-1).  */
  static const double TWO_MANT_DIG =
    /* Assume DBL_MANT_DIG <= 5 * 31.
       Use the identity
       n = floor(n/5) + floor((n+1)/5) + ... + floor((n+4)/5).  */
    (double) (1U << ((DBL_MANT_DIG - 1) / 5))
    * (double) (1U << ((DBL_MANT_DIG - 1 + 1) / 5))
    * (double) (1U << ((DBL_MANT_DIG - 1 + 2) / 5))
    * (double) (1U << ((DBL_MANT_DIG - 1 + 3) / 5))
    * (double) (1U << ((DBL_MANT_DIG - 1 + 4) / 5));

  /* The use of 'volatile' guarantees that excess precision bits are dropped at
     each addition step and before the following comparison at the caller's
     site.  It is necessary on x86 systems where double-floats are not IEEE
     compliant by default, to avoid that the results become platform and
     compiler option dependent.  'volatile' is a portable alternative to gcc's
     -ffloat-store option.  */
  volatile double y = x;
  volatile double z = y;

  if (z > 0.0)
    {
      /* Avoid rounding error for x = 0.5 - 2^(-DBL_MANT_DIG-1).  */
      if (z < 0.5)
        z = 0.0;
      /* Avoid rounding errors for values near 2^k, where k >= DBL_MANT_DIG-1.  */
      else if (z < TWO_MANT_DIG)
        {
          /* Add 0.5 to the absolute value.  */
          y = z += 0.5;
          /* Round to the next integer (nearest or up or down, doesn't
             matter).  */
          z += TWO_MANT_DIG;
          z -= TWO_MANT_DIG;
          /* Enforce rounding down.  */
          if (z > y)
            z -= 1.0;
        }
    }
  else if (z < 0.0)
    {
      /* Avoid rounding error for x = -(0.5 - 2^(-DBL_MANT_DIG-1)).  */
      if (z > - 0.5)
        z = MINUS_ZERO;
      /* Avoid rounding errors for values near -2^k, where k >= DBL_MANT_DIG-1.  */
      else if (z > -TWO_MANT_DIG)
        {
          /* Add 0.5 to the absolute value.  */
          y = z -= 0.5;
          /* Round to the next integer (nearest or up or down, doesn't
             matter).  */
          z -= TWO_MANT_DIG;
          z += TWO_MANT_DIG;
          /* Enforce rounding up.  */
          if (z < y)
            z += 1.0;
        }
    }
  return z;
}

// adapted from Lokinet llarp/encode.hpp
// from  https://en.wikipedia.org/wiki/Base32#z-base-32
static const char zbase32_alpha[] = {'y', 'b', 'n', 'd', 'r', 'f', 'g', '8',
                                     'e', 'j', 'k', 'm', 'c', 'p', 'q', 'x',
                                     'o', 't', '1', 'u', 'w', 'i', 's', 'z',
                                     'a', '3', '4', '5', 'h', '7', '6', '9'};

/// adapted from i2pd
template <typename v, typename stack_t>
const char* base32z_encode(const v& value, stack_t &stack)
{
  size_t ret = 0, pos = 1;
  int bits = 8, tmp = value[0];
  size_t len = value.size();
  while(ret < sizeof(stack) && (bits > 0 || pos < len))
  {
    if(bits < 5)
    {
      if(pos < len)
      {
        tmp <<= 8;
        tmp |= value[pos] & 0xFF;
        pos++;
        bits += 8;
      }
      else  // last byte
      {
        tmp <<= (5 - bits);
        bits = 5;
      }
    }

    bits -= 5;
    int ind = (tmp >> bits) & 0x1F;
    if(ret < sizeof(stack))
    {
      stack[ret] = zbase32_alpha[ind];
      ret++;
    }
    else
      return nullptr;
  }
  return &stack[0];
}

std::string loki::hex64_to_base32z(const std::string &src)
{
  assert(src.size() <= 64); // NOTE: Developer error, update function if you need more. This is intended for 64 char snode pubkeys
  // decode to binary
  std::vector<uint8_t> bin;
  std::transform(src.begin(), src.end(), std::back_inserter(bin), [](const char & ch) -> uint8_t {
    if(ch >= '0' && ch <= '9')
      return ch - 48;
    else if(ch >= 'A' && ch <= 'F' )
      return ch - 55;
    else if(ch >= 'a' && ch <= 'f' )
      return ch - 87;
    else
      return 0;
  });
  // encode to base32z
  char buf[64] = {0};
  std::string result;
  if (char const *dest = base32z_encode(bin, buf))
    result = dest;

  return result;
}
