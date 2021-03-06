#include "UserMarkHelper.hpp"

#include "map/place_page_info.hpp"

#include "partners_api/ads_engine.hpp"
#include "partners_api/banner.hpp"

#include "base/string_utils.hpp"

namespace usermark_helper
{
using search::AddressInfo;
using feature::Metadata;

void InjectMetadata(JNIEnv * env, jclass const clazz, jobject const mapObject, feature::Metadata const & metadata)
{
  static jmethodID const addId = env->GetMethodID(clazz, "addMetadata", "(ILjava/lang/String;)V");
  ASSERT(addId, ());

  for (auto const t : metadata.GetPresentTypes())
  {
    // TODO: It is not a good idea to pass raw strings to UI. Calling separate getters should be a better way.
    jni::TScopedLocalRef metaString(env, t == feature::Metadata::FMD_WIKIPEDIA ?
                                              jni::ToJavaString(env, metadata.GetWikiURL()) :
                                              jni::ToJavaString(env, metadata.Get(t)));
    env->CallVoidMethod(mapObject, addId, t, metaString.get());
  }
}

jobject CreateBanner(JNIEnv * env, std::string const & id, jint type)
{
  static jmethodID const bannerCtorId =
      jni::GetConstructorID(env, g_bannerClazz, "(Ljava/lang/String;I)V");

  return env->NewObject(g_bannerClazz, bannerCtorId, jni::ToJavaString(env, id), type);
}

jobject CreateMapObject(JNIEnv * env, string const & mwmName, int64_t mwmVersion,
                        uint32_t featureIndex, int mapObjectType, string const & title,
                        string const & secondaryTitle, string const & subtitle, double lat,
                        double lon, string const & address, Metadata const & metadata,
                        string const & apiId, jobjectArray jbanners, bool isReachableByTaxi,
                        string const & bookingSearchUrl, jobject const & localAdInfo,
                        jobject const & routingPointInfo)
{
  // public MapObject(@NonNull String mwmName, long mwmVersion, int featureIndex,
  //                  @MapObjectType int mapObjectType, String title, @Nullable String secondaryTitle,
  //                  String subtitle, String address, double lat, double lon, String apiId,
  //                  @Nullable Banner[] banners, boolean reachableByTaxi,
  //                  @Nullable String bookingSearchUrl, @Nullable LocalAdInfo localAdInfo,
  //                  @Nullable RoutePointInfo routePointInfo)
  static jmethodID const ctorId =
      jni::GetConstructorID(env, g_mapObjectClazz,
                            "(Ljava/lang/String;JIILjava/lang/String;Ljava/lang/"
                            "String;Ljava/lang/String;Ljava/lang/String;DDLjava/lang/"
                            "String;[Lcom/mapswithme/maps/ads/Banner;ZLjava/lang/String;"
                            "Lcom/mapswithme/maps/ads/LocalAdInfo;"
                            "Lcom/mapswithme/maps/routing/RoutePointInfo;)V");

  jni::TScopedLocalRef jMwmName(env, jni::ToJavaString(env, mwmName));
  jni::TScopedLocalRef jTitle(env, jni::ToJavaString(env, title));
  jni::TScopedLocalRef jSecondaryTitle(env, jni::ToJavaString(env, secondaryTitle));
  jni::TScopedLocalRef jSubtitle(env, jni::ToJavaString(env, subtitle));
  jni::TScopedLocalRef jAddress(env, jni::ToJavaString(env, address));
  jni::TScopedLocalRef jApiId(env, jni::ToJavaString(env, apiId));
  jni::TScopedLocalRef jBookingSearchUrl(env, jni::ToJavaString(env, bookingSearchUrl));
  jobject mapObject = env->NewObject(
      g_mapObjectClazz, ctorId, jMwmName.get(), (jlong)mwmVersion, (jint)featureIndex,
      mapObjectType, jTitle.get(), jSecondaryTitle.get(), jSubtitle.get(), jAddress.get(),
      lat, lon, jApiId.get(), jbanners, isReachableByTaxi, jBookingSearchUrl.get(),
      localAdInfo, routingPointInfo);

  InjectMetadata(env, g_mapObjectClazz, mapObject, metadata);
  return mapObject;
}

jobject CreateMapObject(JNIEnv * env, place_page::Info const & info)
{
  jni::TScopedLocalObjectArrayRef jbanners(env, nullptr);
  if (info.HasBanner())
    jbanners.reset(ToBannersArray(env, info.GetBanners()));

  jni::TScopedLocalRef localAdInfo(env, CreateLocalAdInfo(env, info));

  jni::TScopedLocalRef routingPointInfo(env, nullptr);
  if (info.m_isRoutePoint)
    routingPointInfo.reset(CreateRoutePointInfo(env, info));

  if (info.IsBookmark())
  {
    // public Bookmark(@NonNull String mwmName, long mwmVersion, int featureIndex,
    //                 @IntRange(from = 0) int categoryId, @IntRange(from = 0) int bookmarkId,
    //                 String title, @Nullable String secondaryTitle, @Nullable String objectTitle,
    //                 @Nullable Banner[] banners, boolean reachableByTaxi,
    //                 @Nullable String bookingSearchUrl, @Nullable LocalAdInfo localAdInfo,
    //                 @Nullable RoutePointInfo routePointInfo)
    static jmethodID const ctorId =
        jni::GetConstructorID(env, g_bookmarkClazz,
                              "(Ljava/lang/String;JIIILjava/lang/String;Ljava/"
                              "lang/String;Ljava/lang/String;"
                              "[Lcom/mapswithme/maps/ads/Banner;ZLjava/lang/String;"
                              "Lcom/mapswithme/maps/ads/LocalAdInfo;"
                              "Lcom/mapswithme/maps/routing/RoutePointInfo;)V");

    auto const & bac = info.GetBookmarkAndCategory();
    BookmarkCategory * cat = g_framework->NativeFramework()->GetBmCategory(bac.m_categoryIndex);
    BookmarkData const & data =
        static_cast<Bookmark const *>(cat->GetUserMark(bac.m_bookmarkIndex))->GetData();

    jni::TScopedLocalRef jMwmName(env, jni::ToJavaString(env, info.GetID().GetMwmName()));
    jni::TScopedLocalRef jName(env, jni::ToJavaString(env, data.GetName()));
    jni::TScopedLocalRef jTitle(env, jni::ToJavaString(env, info.GetTitle()));
    jni::TScopedLocalRef jSecondaryTitle(env, jni::ToJavaString(env, info.GetSecondaryTitle()));
    jni::TScopedLocalRef jBookingSearchUrl(env, jni::ToJavaString(env, info.GetBookingSearchUrl()));
    jobject mapObject =
        env->NewObject(g_bookmarkClazz, ctorId, jMwmName.get(), info.GetID().GetMwmVersion(),
                       info.GetID().m_index, static_cast<jint>(info.m_bac.m_categoryIndex),
                       static_cast<jint>(info.m_bac.m_bookmarkIndex), jName.get(), jTitle.get(),
                       jSecondaryTitle.get(), jbanners.get(), info.IsReachableByTaxi(),
                       jBookingSearchUrl.get(), localAdInfo.get(), routingPointInfo.get());
    if (info.IsFeature())
      InjectMetadata(env, g_mapObjectClazz, mapObject, info.GetMetadata());
    return mapObject;
  }

  ms::LatLon const ll = info.GetLatLon();
  search::AddressInfo const address =
      g_framework->NativeFramework()->GetAddressInfoAtPoint(info.GetMercator());

  // TODO(yunikkk): object can be POI + API + search result + bookmark simultaneously.
  // TODO(yunikkk): Should we pass localized strings here and in other methods as byte arrays?
  if (info.IsMyPosition())
  {
    return CreateMapObject(env, info.GetID().GetMwmName(), info.GetID().GetMwmVersion(),
                           info.GetID().m_index, kMyPosition, info.GetTitle(),
                           info.GetSecondaryTitle(), info.GetSubtitle(), ll.lat, ll.lon,
                           address.FormatAddress(), {}, "", jbanners.get(),
                           info.IsReachableByTaxi(), info.GetBookingSearchUrl(),
                           localAdInfo.get(), routingPointInfo.get());
  }

  if (info.HasApiUrl())
  {
    return CreateMapObject(env, info.GetID().GetMwmName(), info.GetID().GetMwmVersion(),
                           info.GetID().m_index, kApiPoint, info.GetTitle(),
                           info.GetSecondaryTitle(), info.GetSubtitle(), ll.lat, ll.lon,
                           address.FormatAddress(), info.GetMetadata(), info.GetApiUrl(),
                           jbanners.get(), info.IsReachableByTaxi(), info.GetBookingSearchUrl(),
                           localAdInfo.get(), routingPointInfo.get());
  }

  return CreateMapObject(env, info.GetID().GetMwmName(), info.GetID().GetMwmVersion(),
                         info.GetID().m_index, kPoi, info.GetTitle(), info.GetSecondaryTitle(),
                         info.GetSubtitle(), ll.lat, ll.lon, address.FormatAddress(),
                         info.IsFeature() ? info.GetMetadata() : Metadata(), "", jbanners.get(),
                         info.IsReachableByTaxi(), info.GetBookingSearchUrl(), localAdInfo.get(),
                         routingPointInfo.get());
}

jobjectArray ToBannersArray(JNIEnv * env, vector<ads::Banner> const & banners)
{
  return jni::ToJavaArray(env, g_bannerClazz, banners,
                          [](JNIEnv * env, ads::Banner const & item) {
                            return CreateBanner(env, item.m_bannerId, static_cast<jint>(item.m_type));
                          });
}

jobject CreateLocalAdInfo(JNIEnv * env, place_page::Info const & info)
{
  static jclass const localAdInfoClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/ads/LocalAdInfo");
  static jmethodID const ctorId = jni::GetConstructorID(env, localAdInfoClazz,
                                                        "(ILjava/lang/String;)V");

  jni::TScopedLocalRef jLocalAdUrl(env, jni::ToJavaString(env, info.GetLocalAdsUrl()));

  return env->NewObject(localAdInfoClazz, ctorId, info.GetLocalAdsStatus(), jLocalAdUrl.get());
}

jobject CreateRoutePointInfo(JNIEnv * env, place_page::Info const & info)
{
  static jclass const clazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/routing/RoutePointInfo");
  static jmethodID const ctorId = jni::GetConstructorID(env, clazz, "(II)V");
  int const markType = static_cast<int>(info.m_routeMarkType);
  return env->NewObject(clazz, ctorId, markType, info.m_intermediateIndex);
}
}  // namespace usermark_helper
